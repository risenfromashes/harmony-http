#pragma once

#include <functional>

#include "dbresult.h"
#include "dbsession.h"

namespace hm {
class Stream;
}

namespace hm::db {
// light wrapper around db session and stream
class Connection {

public:
  void query(const char *command, std::function<void(db::Result)> &&cb);

  void query_params(const char *command,
                    std::initializer_list<std::string> params,
                    std::function<void(db::Result)> &&cb);

  Connection(Stream *stream);

private:
  Stream *stream_;
};

}; // namespace hm::db
