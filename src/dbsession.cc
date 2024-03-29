#include <cstring>
#include <fcntl.h>
#include <iterator>
#include <postgresql/libpq-fe.h>

#include "awaitabletask.h"
#include "dbquery.h"
#include "dbresult.h"
#include "dbsession.h"
#include "eventdispatcher.h"
#include "stream.h"
#include "worker.h"

#include <iostream>
#include <variant>

namespace hm::db {

Session::Session(Worker *worker, PGconn *conn, const char *default_query_dir)
    : connected_(false), worker_(worker), loop_(worker->get_loop()),
      conn_(conn),
      query_dir_(default_query_dir) { // likely to change during connection
  fd_ = -1;                           // keep it unitialised
  ev_init(&rev_, read_cb);
  ev_init(&wev_, write_cb);
  rev_.data = this;
  wev_.data = this;
  read_fn_ = &Session::poll_connection;
  write_fn_ = &Session::poll_connection;
  dir_fd_ = -1;
  if (query_dir_) {
    dir_fd_ = open(default_query_dir, O_RDONLY);
    if (dir_fd_ < 0) {
      std::cerr << "Query directory: [" << query_dir_ << "] not found."
                << std::endl;
    }
  }
  poll_connection();
}

std::unique_ptr<Session> Session::create(Worker *worker,
                                         const char *connection_str,
                                         const char *query_dir) {
  PGconn *conn = PQconnectStart(connection_str);
  if (!conn) {
    std::cerr << "Failed to create PGconn struct" << std::endl;
    return nullptr;
  }

  if (PQstatus(conn) == CONNECTION_BAD) {
    std::cerr << "Bad connection: invalid connection parameters most likely"
              << std::endl;
    PQfinish(conn);
    return nullptr;
  }
  return std::unique_ptr<Session>(new Session(worker, conn, query_dir));
}

Session::~Session() {
  PQfinish(conn_);
  ev_io_stop(loop_, &rev_);
  ev_io_stop(loop_, &wev_);
  if (dir_fd_ >= 0) {
    close(dir_fd_);
  }
}

int Session::send_query(db::Query &query) {
  int rv = -1;
  if (std::holds_alternative<QueryArg>(query.arg)) {
    auto &arg = std::get<QueryArg>(query.arg);
    rv = PQsendQuery(conn_, arg.command);
  } else if (std::holds_alternative<QueryParamArg>(query.arg)) {
    auto &arg = std::get<QueryParamArg>(query.arg);

    assert(arg.param_vector.size() <= 20 && "parameter size too large");
    const char *values[20];
    for (size_t i = 0; i < arg.param_vector.size(); i++) {
      values[i] = arg.param_vector[i].c_str();
    }

    rv = PQsendQueryParams(conn_, arg.command, arg.param_vector.size(),
                           /*  implict types */ nullptr,
                           /* value */ values,
                           /* length not required in text format */ nullptr,
                           /* all in text format */ nullptr,
                           /* result in text format */ 0);
  } else if (std::holds_alternative<PrepareArg>(query.arg)) {
    auto &arg = std::get<PrepareArg>(query.arg);
    if (arg.file_fd >= 0) {
      // read file
      auto size = lseek(arg.file_fd, 0, SEEK_END);
      char *buf = new char[size + 1];
      pread(arg.file_fd, buf, size, 0);
      buf[size] = '\0';
      rv = PQsendPrepare(conn_, arg.statement, buf, 0,
                         /* param types implicit */ nullptr);
      delete[] buf;
    } else {
      assert(arg.query != nullptr);
      rv = PQsendPrepare(conn_, arg.statement, arg.query, 0,
                         /* param types implicit */ nullptr);
    }

  } else if (std::holds_alternative<QueryPreparedArg>(query.arg)) {
    auto &arg = std::get<QueryPreparedArg>(query.arg);

    assert(arg.param_vector.size() <= 20 && "parameter size too large");
    const char *values[20];
    for (size_t i = 0; i < arg.param_vector.size(); i++) {
      values[i] = arg.param_vector[i].c_str();
    }

    rv = PQsendQueryPrepared(conn_, arg.statement, arg.param_vector.size(),
                             /* values */ values,
                             /* lengths of text data implicit*/ nullptr,
                             /*all in text format */ nullptr, 0);
  }
  return rv;
}

void Session::read_cb(struct ev_loop *loop, ev_io *w, int revents) {
  auto *db = static_cast<Session *>(w->data);
  int rv = db->on_read();
  if (rv == -1) {
    std::cerr << "Irrecoverable error occured in database session."
              << std::endl;
    db->worker_->restart_db_session();
  }
}

void Session::write_cb(struct ev_loop *loop, ev_io *w, int revents) {
  auto *db = static_cast<Session *>(w->data);
  int rv = db->on_write();
  if (rv == -1) {
    std::cerr << "Irrecoverable error occured in database session."
              << std::endl;
    db->worker_->restart_db_session();
  }
}

int Session::poll_connection() {
  switch (PQconnectPoll(conn_)) {
  case PGRES_POLLING_ACTIVE:
    return poll_connection();
  case PGRES_POLLING_READING:
    // stop polling for write
    ev_io_stop(loop_, &wev_);
    if (int new_fd = PQsocket(conn_); new_fd != fd_) {
      // update socket
      fd_ = new_fd;
      ev_io_stop(loop_, &rev_);
      ev_io_set(&rev_, fd_, EV_READ);
      ev_io_set(&wev_, fd_, EV_WRITE);
    }
    ev_io_start(loop_, &rev_);
    break;
  case PGRES_POLLING_WRITING:
    if (int new_fd = PQsocket(conn_); new_fd != fd_) {
      // update socket
      fd_ = new_fd;
      ev_io_stop(loop_, &rev_);
      ev_io_stop(loop_, &wev_);
      ev_io_set(&rev_, fd_, EV_READ);
      ev_io_set(&wev_, fd_, EV_WRITE);
    }
    ev_io_start(loop_, &wev_);
    break;
  case PGRES_POLLING_OK:
    if (connection_made() != 0) {
      return -1;
    }
    break;
  case PGRES_POLLING_FAILED:
    std::cerr << "DBConnection failed." << std::endl;
    std::cerr << "Error: " << PQerrorMessage(conn_) << std::endl;
    return -1;
  }
  return 0;
}

int Session::connection_made() {
  int rv;
  read_fn_ = &Session::read;
  write_fn_ = &Session::write;
  rv = PQsetnonblocking(conn_, 1);
  if (rv != 0) {
    std::cerr << "Failed to set DB socket to non-blocking" << std::endl;
    std::cerr << PQerrorMessage(conn_) << std::endl;
    return -1;
  }
  rv = PQenterPipelineMode(conn_);
  if (rv != 1) {
    std::cerr << "Failed to enter pipeline mode" << std::endl;
    return -1;
  }
  connected_ = true;
  return 0;
}

static void handle_result(DispatchedQuery &query, Result &&result, bool alive,
                          bool resume = true) {
  if (alive) {
    auto &handler = query.completion_handler;
    if (std::holds_alternative<std::coroutine_handle<>>(handler)) {
      auto c = std::get<std::coroutine_handle<>>(handler);
      auto coro =
          std::coroutine_handle<AwaitableTask<Result>::Promise>::from_address(
              c.address());
      coro.promise().value.emplace(std::move(result));
      if (!c.done() && resume) {
        coro.resume();
      }
    } else {
      auto &t = std::get<std::function<void(Result)>>(handler);
      t(std::move(result));
    }
  }
}

void Session::check_notif() {
  PGnotify *notif = PQnotifies(conn_);
  while (notif) {
    // std::cerr << "Got notification!" << std::endl;
    // std::cerr << "channel: " << notif->relname << std::endl;
    // std::cerr << "payload: " << notif->extra << std::endl;
    EventDispatcher::dispatch(Event(notif->relname, SharedNotify(notif)));
    notif = PQnotifies(conn_);
  }
}

int Session::read() {
  int rv = PQconsumeInput(conn_);

  if (rv == 0) {
    std::cerr << "Error trying to consume input." << std::endl;
    std::cerr << PQerrorMessage(conn_) << std::endl;
    return -1;
  }

  check_notif();

  while (PQisBusy(conn_) == 0) {
    // results arrived for at list one query
    auto &query = dispatched_.front();

    bool pipeline_sync = false;
    bool error = false;

    PGresult *result;

    bool stream_alive = worker_->is_stream_alive(query.stream_serial);

    // until there's a nullptr this query isn't finished
    while ((result = PQgetResult(conn_)) != nullptr) {

      switch (PQresultStatus(result)) {
      case PGRES_COMMAND_OK:
      case PGRES_TUPLES_OK:
      case PGRES_SINGLE_TUPLE:
        handle_result(query, result, stream_alive);
        break;

      case PGRES_NONFATAL_ERROR:
      case PGRES_FATAL_ERROR:
      case PGRES_PIPELINE_ABORTED:
        // will be handled as error
        handle_result(query, result, stream_alive);
        break;

      case PGRES_EMPTY_QUERY:
        std::cerr << "Application sent empty query" << std::endl;
        PQclear(result);
        break;
      case PGRES_BAD_RESPONSE:
        std::cerr << "Server sent bad response" << std::endl;
        error = true;
        PQclear(result);
        break;
      case PGRES_PIPELINE_SYNC:
        pipeline_sync = true;
        PQclear(result);
        break;
      case PGRES_COPY_IN:
      case PGRES_COPY_OUT:
      case PGRES_COPY_BOTH:
        std::cerr << "Error: No copy operations should occur" << std::endl;
        error = true;
        PQclear(result);
        break;
      }
      if (pipeline_sync) {
        // if this is pipeline_sync then no need to wait for nullptr
        break;
      }
    }

    if (!pipeline_sync && !dispatched_.empty()) {
      dispatched_.pop_front();
    }

    if (error) {
      return -1;
    }

    check_notif();
    if (dispatched_.empty()) {
      // everything read already
      break;
    }
  }
  return 0;
}

int Session::write() {
  for (;;) {
    int rv = PQflush(conn_);
    if (rv == 1) {
      return 0; // would block
    } else if (rv == -1) {
      std::cerr << "PQflush failed" << std::endl;
      std::cerr << PQerrorMessage(conn_) << std::endl;
      return -1;
    }
    int n = 20; // send queries in batches of 20
    while (!queued_.empty() && (n--)) {
      auto &query = queued_.front();
      int rv = send_query(query);
      if (rv == 0) {
        std::cerr << "Submitting query failed" << std::endl;
        std::cerr << PQerrorMessage(conn_) << std::endl;
        return -1;
      }
      if (query.is_sync_point) {
        int rv = PQpipelineSync(conn_);
        if (rv == 0) {
          std::cerr << "Pipeline sync request failed" << std::endl;
          return -1;
        }
      }
      dispatched_.emplace_back(db::DispatchedQuery{
          .stream_serial = query.stream_serial,
          .is_sync_point = query.is_sync_point,
          .completion_handler = std::move(query.completion_handler)});
      queued_.pop_front();
    }

    if (queued_.empty()) {
      break;
    }
  }
  // wrote everything
  ev_io_stop(loop_, &wev_);
  return 0;
}

void Session::send_query(Stream *stream, const char *command,
                         completion_handler &&cb) {
  queued_.emplace_back(db::Query{.stream_serial = stream->serial(),
                                 .is_sync_point = true,
                                 .arg = db::QueryArg{.command = command},
                                 .completion_handler = std::move(cb)});
  // sending query start writing
  ev_io_start(loop_, &wev_);
}

void Session::send_query_params(Stream *stream, const char *command,
                                std::vector<std::string> &&vec,
                                completion_handler &&cb) {
  auto &query = queued_.emplace_back(
      db::Query{.stream_serial = stream->serial(),
                .is_sync_point = true,
                .arg = db::QueryParamArg{.command = command,
                                         .param_vector = std::move(vec)},
                .completion_handler = std::move(cb)});
  // sending query start writing
  ev_io_start(loop_, &wev_);
}

void Session::send_prepared(Stream *stream, const char *statement,
                            const char *command, completion_handler &&cb) {
  add_prepared_query(statement);
  queued_.emplace_back(db::Query{.stream_serial = stream->serial(),
                                 .is_sync_point = true,
                                 .arg = db::PrepareArg{.statement = statement,
                                                       .query = command,
                                                       .file_fd = -1},
                                 .completion_handler = std::move(cb)});
  // sending query start writing
  ev_io_start(loop_, &wev_);
}

void Session::send_prepared(Stream *stream, const char *statement, int file_fd,
                            completion_handler &&cb) {
  assert(file_fd >= 0);
  add_prepared_query(statement);
  queued_.emplace_back(db::Query{.stream_serial = stream->serial(),
                                 .is_sync_point = true,
                                 .arg = db::PrepareArg{.statement = statement,
                                                       .query = nullptr,
                                                       .file_fd = file_fd},
                                 .completion_handler = std::move(cb)});
  // sending query start writing
  ev_io_start(loop_, &wev_);
}

void Session::send_query_prepared(Stream *stream, const char *statement,
                                  std::vector<std::string> &&params,
                                  completion_handler &&cb) {

  queued_.emplace_back(
      db::Query{.stream_serial = stream->serial(),
                .is_sync_point = true,
                .arg = db::QueryPreparedArg{.statement = statement,
                                            .param_vector = std::move(params)},
                .completion_handler = std::move(cb)});
  // sending query start writing
  ev_io_start(loop_, &wev_);
}

void Session::add_prepared_query(const char *statement) {
  prepared_queries_.emplace(statement);
}

bool Session::check_prepared_query(const char *statement) {
  return prepared_queries_.contains(statement);
}

int Session::open_query_file(const char *statement) {
  char name_buf[64];
  int file_fd;
  assert(std::strlen(statement) + 4 < 64 && "File name too large");
  std::strcpy(name_buf, statement);
  std::strcat(name_buf, ".sql");
  if (dir_fd_ >= 0) {
    file_fd = openat(dir_fd_, name_buf, O_RDONLY);
  } else {
    file_fd = open(name_buf, O_RDONLY);
  }
  return file_fd;
}

void Session::set_query_location(const char *dir) {
  query_dir_ = dir;
  if (dir_fd_ >= 0) {
    close(dir_fd_);
    dir_fd_ = -1;
  }
  dir_fd_ = open(dir, O_RDONLY);
  if (dir_fd_ < 0) {
    std::cerr << "Query directory: [" << dir << "] not found." << std::endl;
  }
}

} // namespace hm::db
