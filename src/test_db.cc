/* A simple cli to understand the libpq pipeline interface */

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ev.h>
#include <postgresql/libpq-fe.h>

#include <cassert>

#include <iostream>
#include <queue>
#include <string>
#include <unistd.h>

enum DBQueryType { QUERY, QUERY_PARAMS, PREPARE, QUERY_PREPARED };

struct DBQuery {
  bool is_sync_point;

  std::string command_buffer;

  DBQueryType query_type;
  union {
    struct {
      const char *command;
    } query;

    struct {
      const char *command;
      int n_params;
      const Oid *param_types;
      const char *const *param_values;
      const int *param_lengths;
      const int *param_formats;
      int result_format;
    } query_params;

    struct {
      const char *statement;
      const char *query;
      int n_params;
      const Oid *param_types;
    } prepare;

    struct {
      const char *statement;
      int n_params;
      const char *const *param_values;
      const int *param_lengths;
      const int *param_formats;
      int result_format;
    } query_prepared;

  } args;

  void (*result_cb)(PGresult *);
  void (*error_cb)(PGresult *);
};

struct DBSession {
  PGconn *conn;
  int fd;
  ev_io wev;
  ev_io rev;
  ev_io stdin;
  int (*read_fn)(DBSession *db, struct ev_loop *);
  int (*write_fn)(DBSession *db, struct ev_loop *);

  std::deque<DBQuery> queued;
  std::deque<DBQuery> dispatched;

  std::string cmd_buffer;
};

void read_cb(struct ev_loop *loop, ev_io *w, int revents) {
  auto *db = static_cast<DBSession *>(w->data);
  int rv = db->read_fn(db, loop);
  if (rv == -1) {
    ev_break(loop);
  }
}

void write_cb(struct ev_loop *loop, ev_io *w, int revents) {
  auto *db = static_cast<DBSession *>(w->data);
  int rv = db->write_fn(db, loop);
  if (rv == -1) {
    ev_break(loop);
  }
}

int read(DBSession *db, struct ev_loop *loop) {
  int rv = PQconsumeInput(db->conn);

  if (rv == 0) {
    std::cerr << "Error trying to consume input." << std::endl;
    std::cerr << PQerrorMessage(db->conn) << std::endl;
    return -1;
  }

  while (PQisBusy(db->conn) == 0) {
    // results arrived for at list one query
    auto &query = db->dispatched.front();

    bool pipeline_sync = false;
    bool error = false;

    PGresult *result;

    // until there's a nullptr this query isn't finished
    while ((result = PQgetResult(db->conn)) != nullptr) {
      switch (PQresultStatus(result)) {
      case PGRES_EMPTY_QUERY:
        std::cerr << "Application sent empty query" << std::endl;
        break;
      case PGRES_COMMAND_OK:
        query.result_cb(result);
        break;
      case PGRES_TUPLES_OK:
        query.result_cb(result);
        break;
      case PGRES_BAD_RESPONSE:
        std::cerr << "Server sent bad response" << std::endl;
        error = true;
        break;
      case PGRES_NONFATAL_ERROR:
        std::cerr << "Encountered non-fatal error:" << std::endl;
        std::cerr << PQresultErrorMessage(result) << std::endl;
        break;
      case PGRES_FATAL_ERROR:
        std::cerr << "Encountered fatal error:" << std::endl;
        // std::cerr << PQresultErrorMessage(result) << std::endl;
        query.error_cb(result);
        break;
      case PGRES_SINGLE_TUPLE:
        query.result_cb(result);
        break;
      case PGRES_PIPELINE_ABORTED:
        query.error_cb(result);
        break;
      case PGRES_PIPELINE_SYNC:
        assert(query.is_sync_point &&
               "SYNC must occur on query that is marked sync point");
        pipeline_sync = true;
        break;
      case PGRES_COPY_IN:
      case PGRES_COPY_OUT:
      case PGRES_COPY_BOTH:
        std::cerr << "Error: No copy operations should occur" << std::endl;
        error = true;
        break;
      }
      if (pipeline_sync) {
        // if this is pipeline_sync then no need to wait for nullptr
        break;
      }
    }

    // wait for pipeline sync before removing the query marked sync
    if (!query.is_sync_point || pipeline_sync) {
      db->dispatched.pop_front();
    }

    PQclear(result);

    if (error) {
      return -1;
    }

    if (db->dispatched.empty()) {
      // everything read already
      break;
    }
  }
  return 0;
}

int send_query(DBQuery *query, PGconn *conn) {
  switch (query->query_type) {
  case QUERY: {
    auto &args = query->args.query;
    return PQsendQuery(conn, args.command);
  }
  case QUERY_PARAMS: {
    auto &args = query->args.query_params;
    return PQsendQueryParams(
        conn, args.command, args.n_params, args.param_types, args.param_values,
        args.param_lengths, args.param_formats, args.result_format);
  }
  case PREPARE: {
    auto &args = query->args.prepare;
    return PQsendPrepare(conn, args.statement, args.query, args.n_params,
                         args.param_types);
  }
  case QUERY_PREPARED: {
    auto &args = query->args.query_prepared;
    return PQsendQueryPrepared(conn, args.statement, args.n_params,
                               args.param_values, args.param_lengths,
                               args.param_formats, args.result_format);
  }
  }
}

int write(DBSession *db, struct ev_loop *loop) {
  for (;;) {
    int rv = PQflush(db->conn);
    if (rv == 1) {
      return 0; // would block
    } else if (rv == -1) {
      std::cerr << "PQflush failed" << std::endl;
      std::cerr << PQerrorMessage(db->conn) << std::endl;
      return -1;
    }

    int n = 20; // send queries in batches of 20
    while (!db->queued.empty() && (n--)) {
      auto &query = db->queued.front();
      int rv = send_query(&query, db->conn);
      if (rv == 0) {
        std::cerr << "Submitting query faild" << std::endl;
        std::cerr << PQerrorMessage(db->conn) << std::endl;
        return -1;
      }
      if (query.is_sync_point) {
        int rv = PQpipelineSync(db->conn);
        if (rv == 0) {
          std::cerr << "Pipeline sync request failed" << std::endl;
          return -1;
        }
      }
      db->dispatched.push_back(std::move(query));
      db->queued.pop_front();
    }

    if (db->queued.empty()) {
      break;
    }
  }
  // wrote everything
  ev_io_stop(loop, &db->wev);
  return 0;
}

int connection_made(DBSession *db) {
  int rv;
  db->read_fn = read;
  db->write_fn = write;
  rv = PQsetnonblocking(db->conn, 1);
  if (rv != 0) {
    std::cerr << "Failed to set DB socket to non-blocking" << std::endl;
    std::cerr << PQerrorMessage(db->conn) << std::endl;
    return -1;
  }
  rv = PQenterPipelineMode(db->conn);
  if (rv != 1) {
    std::cerr << "Failed to enter pipeline mode" << std::endl;
    return -1;
  }
  return 0;
}

int poll_conn(DBSession *db, struct ev_loop *loop) {

  switch (PQconnectPoll(db->conn)) {
  case PGRES_POLLING_ACTIVE:
    return poll_conn(db, loop);
  case PGRES_POLLING_READING:
    // stop polling for write
    ev_io_stop(loop, &db->wev);
    if (int new_fd = PQsocket(db->conn); new_fd != db->fd) {
      // update socket
      db->fd = new_fd;
      ev_io_stop(loop, &db->rev);
      ev_io_set(&db->rev, new_fd, EV_READ);
      ev_io_set(&db->wev, new_fd, EV_WRITE);
    }
    ev_io_start(loop, &db->rev);
    break;
  case PGRES_POLLING_WRITING:
    if (int new_fd = PQsocket(db->conn); new_fd != db->fd) {
      // update socket
      db->fd = new_fd;
      ev_io_stop(loop, &db->rev);
      ev_io_stop(loop, &db->wev);
      ev_io_set(&db->rev, new_fd, EV_READ);
      ev_io_set(&db->wev, new_fd, EV_WRITE);
    }
    ev_io_start(loop, &db->wev);
    break;
  case PGRES_POLLING_OK:
    std::cout << "DBConnection successful!" << std::endl;
    if (connection_made(db) != 0) {
      return -1;
    }
    ev_io_start(loop, &db->stdin);
    std::cout << "sql> ";
    std::cout.flush();
    break;
  case PGRES_POLLING_FAILED:
    std::cerr << "DBConnection failed." << std::endl;
    std::cerr << "Error: " << PQerrorMessage(db->conn) << std::endl;
    return -1;
  }
  return 0;
}

int main() {
  struct ev_loop *loop = ev_default_loop(0);

  DBSession db{.conn = PQconnectStart("postgresql:///testdb1"),
               .fd = -1,
               .read_fn = poll_conn,
               .write_fn = poll_conn};

  ev_init(&db.rev, read_cb);
  ev_init(&db.wev, write_cb);
  db.rev.data = &db;
  db.wev.data = &db;

  if (!db.conn) {
    std::cerr << "Failed to create PGconn struct" << std::endl;
    return 0;
  }

  if (PQstatus(db.conn) == CONNECTION_BAD) {
    std::cerr << "Bad connection: invalid connection parameters most likely"
              << std::endl;
    PQfinish(db.conn);
    return 0;
  }

  poll_conn(&db, loop);

  ev_io_init(
      &db.stdin,
      [](struct ev_loop *loop, ev_io *w, int revents) {
        auto *db = static_cast<DBSession *>(w->data);
        auto &cmd_buffer = db->cmd_buffer;

        std::getline(std::cin, cmd_buffer);

        if (cmd_buffer.size()) {
          // query end
          bool is_sync_point = cmd_buffer.back() == ';';
          db->queued.push_back(DBQuery{
              .is_sync_point = is_sync_point,
              .command_buffer = std::move(cmd_buffer),
              .query_type = QUERY,
              .result_cb =
                  [](PGresult *result) {
                    std::cout << "\r";
                    static char sep[] = "|";
                    if (PQresultStatus(result) == PGRES_TUPLES_OK) {
                      PQprintOpt opt{.header = true,
                                     .align = true,
                                     .standard = false,
                                     .html3 = false,
                                     .expanded = false,
                                     .pager = true,
                                     .fieldSep = sep,
                                     .tableOpt = nullptr,
                                     .caption = nullptr,
                                     .fieldName = nullptr};
                      PQprint(stdout, result, &opt);
                    } else {
                      std::cout << "Empty response" << std::endl;
                    }
                    std::cout << "sql> ";
                    std::cout.flush();
                  },
              .error_cb =
                  [](PGresult *result) {
                    std::cout << "\r";
                    if (PQresultStatus(result) != PGRES_PIPELINE_ABORTED) {
                      std::cout
                          << "Error occured: " << PQresultErrorMessage(result)
                          << std::endl;
                    } else {
                      std::cout << "Query failed as pipeline was aborted"
                                << std::endl;
                    }
                    std::cout << "sql> ";
                    std::cout.flush();
                  }});
          auto &query = db->queued.back();
          query.args.query.command = query.command_buffer.c_str();
        } else {
          ev_io_start(loop, &db->wev);
        }
        std::cout << "sql> ";
        std::cout.flush();
        cmd_buffer.clear();
      },
      STDIN_FILENO, EV_READ);
  db.stdin.data = &db;

  ev_run(loop, 0);
  ev_loop_destroy(loop);
  if (db.conn) {
    PQfinish(db.conn);
  }
}
