#pragma once

#include <string_view>

namespace hm::db {

class Error {

public:
  std::string_view message();

  Error(void *pg_result);
  Error(Error &&);
  Error &operator=(Error &&);
  Error(const Error &);
  Error &operator=(const Error &);
  ~Error();

private:
  void *pg_result_;
};

} // namespace hm::db
