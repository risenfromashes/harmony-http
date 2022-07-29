#include <cctype>
#include <cstdint>
#include <cstring>
#include <dirent.h>

#include <cassert>
#include <concepts>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

enum class HttpMethod { GET, POST, PUT, HEAD, PATCH, DELETE, OPTIONS };

struct RouteNodeData {
  enum class Type { ROOT, CONSTANT, PARAM_INT, PARAM_FLOAT, PARAM_STRING };

  Type type;
  std::string_view label;
};

constexpr bool operator==(const RouteNodeData &a, const RouteNodeData &b) {
  return a.type == b.type &&
         (a.type == RouteNodeData::Type::ROOT || a.label == b.label);
}

struct RouteNode;

constexpr bool operator==(const RouteNode &a, const RouteNodeData &b);
constexpr bool operator==(const RouteNodeData &b, const RouteNode &a);

template <> struct std::hash<RouteNodeData> {
  std::size_t operator()(const RouteNodeData &d) const noexcept {
    auto h1 = std::hash<std::string_view>{}(d.label);
    auto h2 = std::hash<RouteNodeData::Type>{}(d.type);
    return h1 ^ (h2 << 1);
  }
};

struct RouteNode {

  constexpr static size_t npos = static_cast<size_t>(-1);

  RouteNodeData data;
  bool terminal;
  uint32_t methods;
  uint16_t route_index[8]; // there shouldn't be more than 2^16 handlers
  std::vector<RouteNode> children;

  // create empty root
  constexpr RouteNode() {
    data.type = RouteNodeData::Type::ROOT;
    terminal = false;
  }

  constexpr RouteNode(const RouteNodeData &data) : data(data) {}

  constexpr RouteNode(RouteNode &&b)
      : data(b.data), terminal(b.terminal), methods(b.methods),
        children(std::move(b.children)) {
    std::copy(b.route_index, b.route_index + 8, route_index);
  }

  constexpr RouteNode &operator=(RouteNode &&b) {
    data = b.data;
    terminal = b.terminal;
    methods = b.methods;
    children = std::move(b.children);
    std::copy(b.route_index, b.route_index + 8, route_index);
    return *this;
  }

  RouteNode(const RouteNode &) = delete;
  RouteNode &operator=(const RouteNode &) = delete;

  constexpr void insert_path(HttpMethod method, std::string_view path,
                             size_t handler_index) {
    auto m = (uint32_t)method;
    methods |= (1 << m);

    assert(path[0] == '/');
    auto end = path.find_first_of('/', 1);
    auto level = path.substr(1, end - 1);

    auto next_path = end == path.npos ? "/" : path.substr(end);

    using enum RouteNodeData::Type;

    RouteNodeData data;
    if (!level.empty() && level.front() == '{' && level.back() == '}') {
      auto type_beg = level.find(':');
      auto type_end = level.length() - 1;

      if (type_beg == level.npos) {
        data.label = level.substr(1, type_end - 1);
      } else {
        data.label = level.substr(1, type_beg - 1);
      }
      data.type = PARAM_STRING;

      if (type_beg != level.npos) {
        auto type = level.substr(type_beg + 1, type_end - type_beg - 1);
        if (type == "int") {
          data.type = PARAM_INT;
        } else if (type == "float") {
          data.type = PARAM_FLOAT;
        }
      }
    } else {
      data.label = level;
      data.type = CONSTANT;
    }

    bool terminal = next_path == "/";

    auto itr = std::find(children.begin(), children.end(), data);
    if (itr == children.end()) {
      auto &child = children.emplace_back(data);

      child.terminal = terminal;

      if (!terminal) {
        child.insert_path(method, next_path, handler_index);
      } else {
        child.methods = 1 << m;
        child.route_index[m] = handler_index;
      }
    } else {
      auto &child = *itr;
      if (!terminal) {
        child.insert_path(method, next_path, handler_index);
      } else {
        child.terminal = true;
        child.methods |= (1 << m);
        child.route_index[m] = handler_index;
      }
    }
  }

  /* returns handler index */
  constexpr std::size_t match(HttpMethod method, std::string_view path,
                              std::vector<std::string_view> &vars) {

    auto m = (uint32_t)method;

    if ((methods & (1 << m)) == 0) {
      return npos;
    }

    assert(path[0] == '/');
    auto end = path.find_first_of('/', 1);
    auto level = path.substr(1, end - 1);

    auto next_path = end == path.npos ? "/" : path.substr(end);

    // std::cout << "path: " << path << std::endl;
    // std::cout << "next path: " << next_path << std::endl;
    // std::cout << "label: " << data.label << std::endl;

    using enum RouteNodeData::Type;

    switch (data.type) {
    case ROOT: // matches everything
      next_path = path;
      break;
    case CONSTANT: // only matches if level string matches
      if (data.label != level) {
        return npos;
      }
      break;
    case PARAM_INT: // check if its really an int
      for (auto &ch : level) {
        if (!std::isdigit(ch)) {
          return npos;
        }
      }
      vars.emplace_back(level);
      break;
    case PARAM_FLOAT: { // requirements of number + atmost one dot
      int n_dot = 0;
      for (auto &ch : level) {
        if (!std::isdigit(ch) && ch != '.') {
          return npos;
        } else if (ch == '.') {
          if (n_dot++) { // just one decimal point
            return npos;
          }
        }
      }
      vars.emplace_back(level);
    } break;
    case PARAM_STRING:
      // nothing to match here, anything passes
      vars.emplace_back(level);
      break;
    }

    for (auto &child : children) {
      if (auto r = child.match(method, next_path, vars); r != npos) {
        return r;
      }
    }

    if (terminal) {
      return route_index[m];
    }

    switch (data.type) {
    case PARAM_FLOAT:
    case PARAM_INT:
    case PARAM_STRING:
      vars.pop_back();
      break;
    default:
      break;
    }

    return npos;
  }
};

constexpr bool operator==(const RouteNode &a, const RouteNodeData &b) {
  return a.data == b;
}

constexpr bool operator==(const RouteNodeData &b, const RouteNode &a) {
  return a == b;
}

class Router {
public:
  struct HandlerData {
    Router *router;
    size_t handler_index;

    constexpr std::optional<std::string_view>
    get_param(std::string_view label) const {
      auto route = std::string_view(router->route_paths_[handler_index]);
      // goto bracket, match label, repeat
      size_t pos, index = 0;
      while ((pos = route.find('{')) != route.npos) {
        if (route.substr(pos + 1, label.size()) == label) {
          std::cout << "index: " << index << std::endl;
          return router->vars_[index];
        }
        route = route.substr(pos + 1);
        index++;
      }
      return std::nullopt;
    }
  };

public:
  void add_route(HttpMethod method, const char *route_path,
                 std::invocable<HandlerData> auto &&handler) {
    assert(handlers_.size() == route_paths_.size());
    size_t handler_index = handlers_.size();
    handlers_.push_back(std::move(handler));
    route_paths_.push_back(std::move(route_path));
    root_.insert_path(method, route_path, handler_index);
  }

  void dispatch_route(HttpMethod method, std::string_view path) {
    vars_.clear();
    auto index = root_.match(method, path, vars_);
    if (index != RouteNode::npos) {
      HandlerData data{.router = this, .handler_index = index};
      std::invoke(handlers_[index], data);
    } else {
      std::cerr << "Path: " << path << " didn't match any route" << std::endl;
    }
  }

private:
  RouteNode root_;
  std::vector<std::string_view> vars_;
  std::vector<std::function<void(const HandlerData &)>> handlers_;
  std::vector<const char *> route_paths_;
};

int main() {
  Router handler;
  handler.add_route(HttpMethod::GET, "/",
                    [](auto) { std::cout << "GET /" << std::endl; });
  handler.add_route(HttpMethod::GET, "/api",
                    [](auto) { std::cout << "GET /api" << std::endl; });
  handler.add_route(HttpMethod::POST, "/api",
                    [](auto) { std::cout << "POST /api" << std::endl; });

  handler.add_route(HttpMethod::GET, "/api/{id:int}/messages",
                    [](const Router::HandlerData &data) {
                      std::cout << "GET /api/{id}/messages" << std::endl;
                      auto id = data.get_param("id");
                      assert(id.has_value());
                      std::cout << "id: " << id.value() << std::endl;
                    });

  handler.add_route(HttpMethod::POST, "/api/{id:int}/messages/{to:int}/{text}",
                    [](const Router::HandlerData &data) {
                      std::cout << "POST /api/{id}/messages/{to}/{text}"
                                << std::endl;
                      auto id = data.get_param("id");
                      auto to = data.get_param("to");
                      auto text = data.get_param("text");
                      assert(id.has_value() && to.has_value());
                      std::cout << "id: " << id.value() << std::endl;
                      std::cout << "to: " << to.value() << std::endl;
                      std::cout << "text: " << text.value() << std::endl;
                    });

  handler.dispatch_route(HttpMethod::GET, "/");
  handler.dispatch_route(HttpMethod::GET, "/api");
  handler.dispatch_route(HttpMethod::POST, "/api");
  handler.dispatch_route(HttpMethod::GET, "/api/69/messages");
  handler.dispatch_route(HttpMethod::POST, "/api/69/messages/77/blahblah");
}
