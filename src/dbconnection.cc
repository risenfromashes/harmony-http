
#include "dbconnection.h"
#include "awaitabletask.h"
#include "coro.h"
#include "stream.h"

#include <algorithm>
#include <coroutine>
#include <ev.h>
#include <string>

namespace hm::db {

Connection::Connection(Stream *stream) : stream_(stream) {}

void Connection::query(const char *command, std::function<void(Result)> &&cb) {
  // sending query start writing
  auto db = stream_->get_db_session();
  db->send_query(stream_, command, std::move(cb));
}

AwaitableTask<Result> Connection::query(const char *command) {
  auto db = stream_->get_db_session();
  db->send_query(stream_, command, co_await this_coro());
  co_await std::suspend_always{};
  co_return keep_value{};
}

void Connection::query_params(const char *command,
                              std::initializer_list<std::string> params,
                              std::function<void(Result)> &&cb) {
  std::vector<std::string> param_vec;
  std::move(params.begin(), params.end(), std::back_inserter(param_vec));
  auto db = stream_->get_db_session();
  db->send_query_params(stream_, command, std::move(param_vec), std::move(cb));
}

AwaitableTask<Result>
Connection::query_params(const char *command,
                         std::initializer_list<std::string> params) {
  auto db = stream_->get_db_session();
  std::vector<std::string> params_;
  params_.reserve(params.size());
  std::move(params.begin(), params.end(), std::back_inserter(params_));
  db->send_query_params(stream_, command, std::move(params_),
                        co_await this_coro());
  co_await std::suspend_always{};
  co_return keep_value{};
}
} // namespace hm::db
