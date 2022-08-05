
#include "dbconnection.h"
#include "stream.h"

#include <algorithm>
#include <ev.h>
#include <string>

namespace hm::db {

QueryAwaitable::QueryAwaitable(Stream *stream, const char *command)
    : stream(stream) {
  arg = QueryArg{.command = command};
}

QueryAwaitable::QueryAwaitable(Stream *stream, const char *command,
                               std::initializer_list<std::string> params)
    : stream(stream) {
  arg = QueryParamArg{.command = command};
  auto &params_ = std::get<QueryParamArg>(arg).param_vector;
  std::move(params.begin(), params.end(), std::back_inserter(params_));
}

void QueryAwaitable::await_suspend(handle_type handle) {
  this->handle = handle;

  auto db = stream->get_db_session();
  if (std::holds_alternative<QueryArg>(arg)) {

    auto &q = std::get<QueryArg>(arg);
    db->send_query(stream, q.command, this);

  } else if (std::holds_alternative<QueryParamArg>(arg)) {

    auto &q = std::get<QueryParamArg>(arg);
    db->send_query_params(stream, q.command, std::move(q.param_vector), this);
  }
}

Result QueryAwaitable::await_resume() { return result; }

void QueryAwaitable::resume(void *result) {
  this->result = result;
  handle.resume();
}

Connection::Connection(Stream *stream) : stream_(stream) {}

void Connection::query(const char *command, std::function<void(Result)> &&cb) {
  // sending query start writing
  auto db = stream_->get_db_session();
  db->send_query(stream_, command, std::move(cb));
}

QueryAwaitable Connection::query(const char *command) {
  return QueryAwaitable(stream_, command);
}

void Connection::query_params(const char *command,
                              std::initializer_list<std::string> params,
                              std::function<void(Result)> &&cb) {
  std::vector<std::string> param_vec;
  std::move(params.begin(), params.end(), std::back_inserter(param_vec));
  auto db = stream_->get_db_session();
  db->send_query_params(stream_, command, std::move(param_vec), std::move(cb));
}

QueryAwaitable
Connection::query_params(const char *command,
                         std::initializer_list<std::string> params) {
  return QueryAwaitable(stream_, command, params);
}
} // namespace hm::db
