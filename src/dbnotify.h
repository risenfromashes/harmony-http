#pragma once

#include <string_view>

namespace hm::db {

class Notify {
public:
  Notify(void *ptr);
  ~Notify();

  std::string_view channel();

  operator std::string_view();

  size_t length();

private:
  void *pg_notify_;
  std::string_view payload_;
};

}; // namespace hm::db
