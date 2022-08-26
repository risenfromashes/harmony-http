#pragma once

#include "uuid.h"

#include <simdjson.h>
#include <string>
#include <vector>

namespace hm::util {

namespace {
template <class T>
concept HasMemberToString = requires(T a) {
  { a.to_string() } -> std::same_as<std::string>;
};

template <class T>
concept HasStdToString = requires(T a) {
  { std::to_string(a) } -> std::same_as<std::string>;
};

template <class T>
concept HasToString = requires(T a) {
  { to_string(a) } -> std::same_as<std::string>;
};

template <class T>
concept HasExplicitStringCtor = requires(T a) {
  { std::string(a) } -> std::same_as<std::string>;
};

} // namespace

template <class T>
concept StringConvertible =
    std::convertible_to<T, std::string> || HasMemberToString<T> ||
    HasStdToString<T> || HasToString<T> || HasExplicitStringCtor<T>;

template <class T>
void add_to_vector(std::vector<T> &ret, auto &&elem, auto &&...rest) {
  ret.emplace_back(std::forward<decltype(elem)>(elem));
  if constexpr (sizeof...(rest) > 0) {
    add_to_vector(rest..., ret);
  }
}

void add_to_vector(std::vector<std::string> &ret, StringConvertible auto &&elem,
                   StringConvertible auto &&...rest) {
  using T = decltype(elem);

  if constexpr (std::convertible_to<T, std::string>) {
    ret.emplace_back(std::forward<decltype(elem)>(elem));
  } else if constexpr (HasMemberToString<T>) {
    ret.emplace_back(elem.to_string());
  } else if constexpr (HasStdToString<T>) {
    ret.emplace_back(std::to_string(std::forward<decltype(elem)>(elem)));
  } else if constexpr (HasToString<T>) {
    ret.emplace_back(to_string(std::forward<decltype(elem)>(elem)));
  } else {
    ret.emplace_back(std::string(std::forward<decltype(elem)>(elem)));
  }

  if constexpr (sizeof...(rest) > 0) {
    add_to_vector(ret, rest...);
  }
}

template <class T> std::vector<T> make_vector(auto &&...elems) {
  std::vector<T> ret;
  add_to_vector(ret, std::forward<decltype(elems)>(elems)...);
  return ret;
}

uuids::uuid generate_uuid();

simdjson::ondemand::parser *get_json_parser();

std::string_view get_extension(std::string_view mime_type);

} // namespace hm::util
