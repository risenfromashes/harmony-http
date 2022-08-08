#include <iterator>
#include <postgresql/libpq-fe.h>

#include "awaitabletask.h"
#include "dbquery.h"
#include "dbresult.h"
#include "dbsession.h"
#include "stream.h"
#include "worker.h"

#include <iostream>
#include <variant>

namespace hm::db {

Session::Session(Worker *worker, PGconn *conn)
    : connected_(false), worker_(worker), loop_(worker->get_loop()),
      conn_(conn) { // likely to change during connection
  fd_ = -1;         // keep it unitialised
  ev_init(&rev_, read_cb);
  ev_init(&wev_, write_cb);
  rev_.data = this;
  wev_.data = this;
  read_fn_ = &Session::poll_connection;
  write_fn_ = &Session::poll_connection;
  poll_connection();
}

std::unique_ptr<Session> Session::create(Worker *worker,
                                         const char *connection_str) {
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
  return std::unique_ptr<Session>(new Session(worker, conn));
}

Session::~Session() {
  PQfinish(conn_);
  ev_io_stop(loop_, &rev_);
  ev_io_stop(loop_, &wev_);
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
    rv = PQsendPrepare(conn_, arg.statement, arg.query, arg.n_params,
                       /* param types implicit */ nullptr);

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

static void handle_result(DispatchedQuery &query, PGresult *result,
                          bool alive) {
  if (alive) {
    auto &handler = query.completion_handler;
    if (std::holds_alternative<std::coroutine_handle<>>(handler)) {
      auto c = std::get<std::coroutine_handle<>>(handler);
      auto coro =
          std::coroutine_handle<AwaitableTask<Result>::Promise>::from_address(
              c.address());
      coro.promise().value = Result(result);
      coro.resume();
    } else {
      auto &t = std::get<std::function<void(Result)>>(handler);
      t(result);
    }
  } else {
    PQclear(result);
  }
}

int Session::read() {
  int rv = PQconsumeInput(conn_);

  if (rv == 0) {
    std::cerr << "Error trying to consume input." << std::endl;
    std::cerr << PQerrorMessage(conn_) << std::endl;
    return -1;
  }

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
        assert(query.is_sync_point &&
               "SYNC must occur on query that is marked sync point");
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

    // wait for pipeline sync before removing the query marked sync
    if (!query.is_sync_point || pipeline_sync) {
      dispatched_.pop_front();
    }

    if (error) {
      return -1;
    }

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
        std::cerr << "Submitting query faild" << std::endl;
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
                         std::coroutine_handle<> coro) {
  queued_.emplace_back(db::Query{.stream_serial = stream->serial(),
                                 .is_sync_point = true,
                                 .arg = db::QueryArg{.command = command},
                                 .completion_handler = coro});
  // sending query start writing
  ev_io_start(loop_, &wev_);
}

void Session::send_query(Stream *stream, const char *command,
                         std::function<void(Result)> &&cb) {
  queued_.emplace_back(db::Query{.stream_serial = stream->serial(),
                                 .is_sync_point = true,
                                 .arg = db::QueryArg{.command = command},
                                 .completion_handler = std::move(cb)});
  // sending query start writing
  ev_io_start(loop_, &wev_);
}

void Session::send_query_params(Stream *stream, const char *command,
                                std::vector<std::string> &&vec,
                                std::coroutine_handle<> coro) {
  auto &query = queued_.emplace_back(
      db::Query{.stream_serial = stream->serial(),
                .is_sync_point = false,
                .arg = db::QueryParamArg{.command = command,
                                         .param_vector = std::move(vec)},
                .completion_handler = coro});
  // sending query start writing
  ev_io_start(loop_, &wev_);
}
void Session::send_query_params(Stream *stream, const char *command,
                                std::vector<std::string> &&vec,
                                std::function<void(Result)> &&cb) {
  auto &query = queued_.emplace_back(
      db::Query{.stream_serial = stream->serial(),
                .is_sync_point = true,
                .arg = db::QueryParamArg{.command = command,
                                         .param_vector = std::move(vec)},
                .completion_handler = std::move(cb)});
  // sending query start writing
  ev_io_start(loop_, &wev_);
}

} // namespace hm::db
