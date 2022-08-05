
#include "dbconnection.h"
#include "stream.h"

#include <algorithm>
#include <ev.h>
#include <string>

namespace hm::db {

QueryAwaitable::QueryAwaitable(Stream *stream, const char *command) {
  auto db = stream->get_db_session();
  db->send_query(stream, command, this);
}

QueryAwaitable::QueryAwaitable(Stream *stream, const char *command,
                               std::initializer_list<std::string> params) {
  auto db = stream->get_db_session();
  std::vector<std::string> params_;
  params_.reserve(params.size());
  std::move(params.begin(), params.end(), std::back_inserter(params_));
  db->send_query_params(stream, command, std::move(params_), this);
}

void QueryAwaitable::await_suspend(handle_type handle) {
  this->handle = handle;
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
