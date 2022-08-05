#pragma once

#include <functional>
#include <variant>

#include "dbquery.h"
#include "dbresult.h"
#include "task.h"

namespace hm {
class Stream;
}

namespace hm::db {

class Session;

struct QueryAwaitable : public Awaitable<> {
  using base = Awaitable<>;

  handle_type handle;
  void *result;

  void await_suspend(handle_type handle);
  Result await_resume();
  void resume(void *result);

  QueryAwaitable(const QueryAwaitable &) = delete;
  QueryAwaitable &operator=(const QueryAwaitable &) = delete;

  QueryAwaitable(Stream *stream, const char *command);
  QueryAwaitable(Stream *stream, const char *command,
                 std::initializer_list<std::string> params);
};

// light wrapper around db session and stream
class Connection {

public:
  void query(const char *command, std::function<void(db::Result)> &&cb);
  QueryAwaitable query(const char *command);

  void query_params(const char *command,
                    std::initializer_list<std::string> params,
                    std::function<void(db::Result)> &&cb);
  QueryAwaitable query_params(const char *command,
                              std::initializer_list<std::string> params);

  Connection(Stream *stream);

private:
  Stream *stream_;
};

}; // namespace hm::db
