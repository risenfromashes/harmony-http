
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

AwaitableTask<Result> Connection::query(const char *command) {
  auto db = stream_->get_db_session();
  db->send_query(stream_, command, co_await this_coro());
  co_await std::suspend_always{};
  co_return keep_value{};
}

void Connection::query(const char *command, std::function<void(Result)> &&cb) {
  // sending query start writing
  auto db = stream_->get_db_session();
  db->send_query(stream_, command, std::move(cb));
}

AwaitableTask<Result>
Connection::query_params(const char *command,
                         std::vector<std::string> &&params) {
  auto db = stream_->get_db_session();
  db->send_query_params(stream_, command, std::move(params),
                        co_await this_coro());
  co_await std::suspend_always{};
  co_return keep_value{};
}

void Connection::query_params(const char *command,
                              std::vector<std::string> &&params,
                              std::function<void(Result)> &&cb) {
  auto db = stream_->get_db_session();
  db->send_query_params(stream_, command, std::move(params), std::move(cb));
}

AwaitableTask<Result> Connection::prepare(const char *statement,
                                          const char *command) {
  auto db = stream_->get_db_session();
  db->send_prepared(stream_, statement, command, co_await this_coro());
  co_await std::suspend_always{};
  co_return keep_value{};
}

AwaitableTask<Result> Connection::prepare(const char *statement, int fd) {
  auto db = stream_->get_db_session();
  db->send_prepared(stream_, statement, fd, co_await this_coro());
  co_await std::suspend_always{};
  co_return keep_value{};
}

void Connection::prepare(const char *statement, const char *command,
                         std::function<void(db::Result)> &&cb) {
  auto db = stream_->get_db_session();
  db->send_prepared(stream_, statement, command, std::move(cb));
}

void Connection::query_prepared(const char *statement,
                                std::vector<std::string> &&params,
                                std::function<void(Result)> &&cb) {
  auto db = stream_->get_db_session();
  db->send_query_prepared(stream_, statement, std::move(params), std::move(cb));
}

AwaitableTask<Result>
Connection::query_prepared(const char *statement,
                           std::vector<std::string> &&params_) {
  std::cout << "query prepared" << std::endl;
  auto params = std::move(params_);
  auto db = stream_->get_db_session();
  if (!db->check_prepared_query(statement)) {
    int fd = db->open_query_file(statement);
    if (fd < 0) {
      co_return Result(true, "Couldn't find query file");
    }

    std::cout << "fould file" << std::endl;

    auto res = co_await prepare(statement, fd);

    std::cout << "sent prepare query" << std::endl;

    if (res.is_error()) {
      co_return res;
    }
  }

  std::cout << "sending query prepared" << std::endl;

  db->send_query_prepared(stream_, statement, std::move(params),
                          co_await this_coro());
  co_await std::suspend_always{};
  co_return keep_value{};
}

void Connection::set_query_location(const char *dir) {
  auto db = stream_->get_db_session();
  db->set_query_location(dir);
}

} // namespace hm::db
