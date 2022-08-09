#pragma once
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>

#include "common_util.h"

namespace hm::json {

class Array;
class Object;

struct Unquoted {
  std::string_view content;
  Unquoted(std::string_view content) : content(content) {}
};

class Node {
public:
  Node(std::string &ret) : ret_(ret) {}
  void append_quoted_string(std::string_view sv);
  void operator=(auto &&v) &&;
  operator Array() &&;
  operator Object() &&;

private:
  std::string &ret_;
  bool ends_;
};

class Object {
public:
  Object(std::string &ret);
  ~Object();
  Node operator[](std::string_view key);

private:
  std::string &ret_;
};

class Array {
public:
  Array(std::string &ret);
  ~Array();

  Array next_array() { return Array(ret_); }
  Object next_object() { return Object(ret_); }

private:
  std::string &ret_;
};

class Writer {
public:
  Node root();
  std::string string();

private:
  std::string ret_;
};

void Node::operator=(auto &&v) && {
  using T = std::remove_cvref_t<std::decay_t<decltype(v)>>;
  if constexpr (std::same_as<T, std::nullptr_t>) {
    ret_ += "null";
  } else if constexpr (std::integral<T>) {
    char buf[24];
    std::snprintf(buf, 24, "%lli", (int64_t)v);
    ret_ += buf;
  } else if constexpr (std::floating_point<T>) {
    char buf[24];
    std::snprintf(buf, 24, "%lf", (double)v);
    ret_ += buf;
  } else if constexpr (std::same_as<T, Unquoted>) {
    ret_ += v.content;
  } else if constexpr (std::convertible_to<T, std::string_view>) {
    auto sv = std::string_view(std::forward<decltype(v)>(v));
    append_quoted_string(sv);
  } else if constexpr (std::same_as<T, bool>) {
    ret_ += v ? "true" : "false";
  }
  ret_ += ',';
}

}; // namespace hm::json
