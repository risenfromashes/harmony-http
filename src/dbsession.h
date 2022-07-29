#pragma once

#include <ev.h>
#include <postgresql/libpq-fe.h>

#include "dbquery.h"

#include <deque>
#include <memory>

namespace hm {

class Worker;

namespace db {

class Session {

  Session(Worker *worker, PGconn *conn);

public:
  static std::unique_ptr<Session> create(Worker *worker,
                                         const char *connection_str);
  ~Session();

  int send_query(db::Query &query);

  bool connected() { return connected_; }

  void send_query(Stream *stream, const char *command,
                  std::function<void(Result)> &&on_sucess = {},
                  std::function<void(Error)> &&on_error = {});

  void send_query_params(Stream *stream, const char *command,
                         std::initializer_list<std::string> params,
                         std::function<void(Result)> &&on_sucess = {},
                         std::function<void(Error)> &&on_error = {});

private:
  static void read_cb(struct ev_loop *loop, ev_io *w, int revents);
  static void write_cb(struct ev_loop *loop, ev_io *w, int revents);

  inline int on_read() { return (this->*(this->read_fn_))(); }
  inline int on_write() { return (this->*(this->write_fn_))(); }

  int read();
  int write();
  int poll_connection();

  inline int connection_made();

private:
  bool connected_;
  Worker *worker_;
  struct ev_loop *loop_;
  PGconn *conn_;
  int fd_;
  ev_io wev_;
  ev_io rev_;
  int (Session::*read_fn_)();
  int (Session::*write_fn_)();

  std::deque<db::Query> queued_;
  std::deque<db::DispatchedQuery> dispatched_;
};

} // namespace db
} // namespace hm
