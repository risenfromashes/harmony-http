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

// light wrapper around db session and stream
class Connection {

public:
  void query(const char *command, std::function<void(db::Result)> &&cb);
  AwaitableTask<Result> query(const char *command);

  void query_params(const char *command,
                    std::initializer_list<std::string> params,
                    std::function<void(db::Result)> &&cb);

  AwaitableTask<Result> query_params(const char *command,
                                     std::initializer_list<std::string> params);

  Connection(Stream *stream);

private:
  Stream *stream_;
};

}; // namespace hm::db
