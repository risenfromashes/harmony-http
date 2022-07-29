#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <string_view>
#include <vector>

#include "util.h"

namespace hm {

class HttpRequest;
class HttpResponse;

enum class HttpMethod { GET, POST, PUT, HEAD, PATCH, DELETE, OPTIONS };

HttpMethod method_from_string(std::string_view str);

struct RouteNodeData {
  enum class Type { ROOT, CONSTANT, PARAM_INT, PARAM_FLOAT, PARAM_STRING };

  Type type;
  std::string_view label;
};

struct RouteNode;

struct RouteNode {

  constexpr static size_t npos = static_cast<size_t>(-1);

  RouteNodeData data;
  bool terminal;
  uint32_t methods;
  uint16_t route_index[8]; // there shouldn't be more than 2^16 handlers
  std::vector<RouteNode> children;

  // create empty root
  RouteNode();
  RouteNode(const RouteNodeData &data);
  RouteNode(RouteNode &&b);
  RouteNode &operator=(RouteNode &&b);
  RouteNode(const RouteNode &) = delete;
  RouteNode &operator=(const RouteNode &) = delete;

  void insert_path(HttpMethod method, std::string_view path,
                   size_t handler_index);
  /* returns handler index */
  std::size_t match(HttpMethod method, std::string_view path,
                    std::vector<std::string_view> &vars) const;
};

class HttpRouter {
  friend class HttpRequest;

public:
  struct HandlerData {
    HttpRouter *router;
    size_t handler_index;

    constexpr std::optional<std::string_view>
    get_param(std::string_view label) const;
  };

private:
  auto &get_vars_vector() const {
    thread_local std::vector<std::string_view> vars;
    return vars;
  }

public:
  void add_route(HttpMethod method, const char *route_path,
                 std::invocable<HandlerData> auto &&handler);

  bool dispatch_route(HttpMethod method, std::string_view path,
                      HttpRequest *request, HttpResponse *response) const;

private:
  RouteNode root_;
  std::vector<std::function<void(HttpRequest *, HttpResponse *)>> handlers_;
  std::vector<const char *> route_paths_;
};

constexpr bool operator==(const RouteNodeData &a, const RouteNodeData &b) {
  return a.type == b.type &&
         (a.type == RouteNodeData::Type::ROOT || a.label == b.label);
}

constexpr bool operator==(const RouteNode &a, const RouteNodeData &b) {
  return a.data == b;
}

constexpr bool operator==(const RouteNodeData &b, const RouteNode &a) {
  return a == b;
}
} // namespace hm
