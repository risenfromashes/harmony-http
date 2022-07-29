
#include "dbconnection.h"
#include "stream.h"

#include <algorithm>
#include <ev.h>
#include <string>

namespace hm::db {

Connection::Connection(Stream *stream) : stream_(stream) {}

void Connection::query(const char *command, std::function<void(Result)> &&cb) {
  // sending query start writing
  auto db = stream_->get_db_session();
  db->send_query(stream_, command, std::move(cb));
}

void Connection::query_params(const char *command,
                              std::initializer_list<std::string> params,
                              std::function<void(Result)> &&cb) {
  std::vector<std::string> param_vec;
  std::move(params.begin(), params.end(), std::back_inserter(param_vec));
  auto db = stream_->get_db_session();
  db->send_query_params(stream_, command, std::move(param_vec), std::move(cb));
}
} // namespace hm::db
