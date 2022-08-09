#pragma once

#include "dbresult.h"

#include <coroutine>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "awaitabletask.h"

namespace hm {
class Stream;
}

namespace hm::db {

struct QueryArg {
  const char *command;
};

struct QueryParamArg {
  const char *command;
  std::vector<std::string> param_vector;
};

struct PrepareArg {
  const char *statement;
  const char *query;
  int file_fd;
};

struct QueryPreparedArg {
  const char *statement;
  std::vector<std::string> param_vector;
};

struct Query {
  const uint64_t stream_serial;
  bool is_sync_point;
  std::variant<QueryArg, QueryParamArg, PrepareArg, QueryPreparedArg> arg;
  std::variant<std::coroutine_handle<>, std::function<void(Result)>>
      completion_handler;
};

struct DispatchedQuery {
  const uint64_t stream_serial;
  bool is_sync_point;
  std::variant<std::coroutine_handle<>, std::function<void(Result)>>
      completion_handler;
};

} // namespace hm::db
