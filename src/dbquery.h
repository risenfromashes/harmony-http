#pragma once

#include "dbresult.h"

#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace hm {
class Stream;
}

namespace hm::db {

struct Query {
  const uint64_t stream_serial;

  bool is_sync_point;

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
    int n_params;
  };

  struct QueryPreparedArg {
    const char *statement;
    std::vector<std::string> param_vector;
  };

  std::variant<QueryArg, QueryParamArg, PrepareArg, QueryPreparedArg> arg;

  std::function<void(Result)> completion_cb;
};

struct DispatchedQuery {
  const uint64_t stream_serial;
  bool is_sync_point;
  std::function<void(Result)> completion_cb;
};

} // namespace hm::db
