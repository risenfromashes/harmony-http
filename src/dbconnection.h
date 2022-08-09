#pragma once

#include <concepts>
#include <functional>
#include <variant>

#include "common_util.h"
#include "dbquery.h"
#include "dbresult.h"
#include "task.h"

namespace hm {
class Stream;
}

namespace hm::db {

class Session;

// light wrapper around db session and stream
class Connection {

public:
  AwaitableTask<Result> query(const char *command);
  void query(const char *command, std::function<void(db::Result)> &&cb);

  AwaitableTask<Result> query_params(const char *command,
                                     util::StringConvertible auto &&...args);
  AwaitableTask<Result> query_params(const char *command,
                                     std::vector<std::string> &&args);

  void query_params(const char *command, util::StringConvertible auto &&...args,
                    std::function<void(db::Result)> &&cb);
  void query_params(const char *command, std::vector<std::string> &&args,
                    std::function<void(db::Result)> &&cb);

  AwaitableTask<Result> prepare(const char *statement, const char *command);
  void prepare(const char *statement, const char *command,
               std::function<void(db::Result)> &&cb);

  AwaitableTask<Result> query_prepared(const char *statement,
                                       util::StringConvertible auto &&...args);
  AwaitableTask<Result> query_prepared(const char *statement,
                                       std::vector<std::string> &&args);

  void query_prepared(const char *statement,
                      util::StringConvertible auto &&...args,
                      std::function<void(db::Result)> &&cb);
  void query_prepared(const char *statement, std::vector<std::string> &&args,
                      std::function<void(db::Result)> &&cb);

  void set_query_location(const char *dir);

  Connection(Stream *stream);

private:
  AwaitableTask<Result> prepare(const char *statement, int fd);

private:
  Stream *stream_;
};

AwaitableTask<Result>
Connection::query_params(const char *command,
                         util::StringConvertible auto &&...args) {
  return query_params(command, util::make_vector<std::string>(
                                   std::forward<decltype(args)>(args)...));
}

void Connection::query_params(const char *command,
                              util::StringConvertible auto &&...args,
                              std::function<void(db::Result)> &&cb) {
  return query_params(
      command,
      util::make_vector<std::string>(std::forward<decltype(args)>(args)...),
      std::move(cb));
}

AwaitableTask<Result>
Connection::query_prepared(const char *statement,
                           util::StringConvertible auto &&...args) {
  auto vec =
      util::make_vector<std::string>(std::forward<decltype(args)>(args)...);
  return query_prepared(statement, std::move(vec));
}

void Connection::query_prepared(const char *statement,
                                util::StringConvertible auto &&...args,
                                std::function<void(db::Result)> &&cb) {
  return query_prepared(
      statement,
      util::make_vector<std::string>(std::forward<decltype(args)>(args)...),
      std::move(cb));
}

}; // namespace hm::db
