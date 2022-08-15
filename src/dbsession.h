#pragma once

#include <ev.h>
#include <postgresql/libpq-fe.h>

#include "dbquery.h"

#include <deque>
#include <memory>
#include <unordered_set>

#include "dbresult.h"
#include "task.h"

namespace hm {

class Worker;

namespace db {

class Session {

  Session(Worker *worker, PGconn *conn,
          const char *default_query_dir = nullptr);

  using completion_handler =
      std::variant<std::coroutine_handle<>, std::function<void(Result)>>;

public:
  static std::unique_ptr<Session> create(Worker *worker,
                                         const char *connection_str,
                                         const char *query_dir = nullptr);
  ~Session();

  int send_query(db::Query &query);

  bool connected() { return connected_; }

  void send_query(Stream *stream, const char *command,
                  completion_handler &&coro);
  void send_query_params(Stream *stream, const char *command,
                         std::vector<std::string> &&params,
                         completion_handler &&coro);
  void send_prepared(Stream *stream, const char *statement, const char *command,
                     completion_handler &&coro);
  void send_prepared(Stream *stream, const char *statement, int file_fd,
                     completion_handler &&coro);

  // returns true if query is actually sent
  void send_query_prepared(Stream *stream, const char *statement,
                           std::vector<std::string> &&params,
                           completion_handler &&coro);

  void add_prepared_query(const char *statement);

  bool check_prepared_query(const char *statement);

  void set_query_location(const char *dir);

  int open_query_file(const char *statement);

private:
  static void read_cb(struct ev_loop *loop, ev_io *w, int revents);
  static void write_cb(struct ev_loop *loop, ev_io *w, int revents);

  inline int on_read() { return (this->*(this->read_fn_))(); }
  inline int on_write() { return (this->*(this->write_fn_))(); }

  int read();
  int write();
  int poll_connection();
  void check_notif();

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

  std::unordered_set<std::string_view> prepared_queries_;

  const char *query_dir_;
  int dir_fd_;
};

} // namespace db
} // namespace hm
