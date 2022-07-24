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
#include <unordered_map>
#include <vector>

enum class HttpMethod { GET, POST, PUT, HEAD, PATCH, DELETE, OPTIONS };

struct RouteNodeData {
  enum class Type { ROOT, CONSTANT, PARAM_INT, PARAM_FLOAT, PARAM_STRING };

  Type type;
  std::string_view label;
};

template <> struct std::hash<RouteNodeData> {
  std::size_t operator()(const RouteNodeData &d) {
    auto h1 = std::hash<std::string_view>{}(d.label);
    auto h2 = std::hash<RouteNodeData::Type>{}(d.type);
    return h1 ^ (h2 << 1);
  }
};

struct RouteNode {
  RouteNodeData data;
  bool terminal;
  uint32_t methods;
  uint16_t handler_index[8]; // there shouldn't be more than 2^16 handlers

  std::unordered_map<RouteNodeData, RouteNode> children;

  void insert_path(HttpMethod method, std::string_view path,
                   size_t handler_index) {
    assert(path[0] == '/');
    auto end = path.find_first_of('/', 1);
    auto level = path.substr(1, end);

    auto next_path = end == path.npos ? "/" : path.substr(end);

    using enum RouteNodeData::Type;

    if (!level.empty()) {
      RouteNodeData data;
      if (level.front() == '{' && level.back() == '}') {
        auto type_beg = level.find(':');
        auto type_end = level.length() - 1;

        if (type_beg == level.npos) {
          data.label = level.substr(1, type_end);
        } else {
          data.label = level.substr(1, type_beg);
        }
        data.type = PARAM_STRING;

        if (type_beg != level.npos) {
          auto type = level.substr(type_beg + 1, type_end);
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

      if (!children.contains(data)) {
        RouteNode node{.data = data};
        auto [itr, inserted] = children.emplace(data, std::move(node));
        assert(inserted);

        auto &child = itr->second;

        child.terminal = terminal;
        if (!terminal) {
          child.insert_path(method, next_path, handler_index);
        } else {
          auto m = (uint32_t)method;
          child.methods = 1 << m;
          child.handler_index[m] = handler_index;
        }
      } else {
        auto &child = children.find(data)->second;
        if (!terminal) {
          child.insert_path(method, next_path, handler_index);
        } else {
          child.terminal = true;
          auto m = (uint32_t)method;
          child.methods |= (1 << m);
          child.handler_index[m] = handler_index;
        }
      }
    }
  }

  /* returns handler index */
  std::size_t
  match(HttpMethod method, std::string_view path,
        std::vector<std::pair<std::string_view, std::string_view>> &vars) {
    auto npos = static_cast<size_t>(-1);
    assert(path[0] == '/');
    auto end = path.find_first_of('/', 1);
    auto level = path.substr(1, end);
    auto next_path = end == path.npos ? "" : path.substr(end);

    assert(terminal || !next_path.empty());

    using enum RouteNodeData::Type;

    switch (data.type) {
    case ROOT:
      next_path = path;
      break;
    case CONSTANT:
      if (data.label != level) {
        return npos;
      }
      break;
    case PARAM_INT:
      for (auto &ch : level) {
        if (!std::isdigit(ch)) {
          return npos;
        }
      }
      vars.emplace_back(data.label, level);
      break;
    case PARAM_FLOAT: {
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
      vars.emplace_back(data.label, level);
    } break;
    case PARAM_STRING:
      // nothing to match here, anything passes
      vars.emplace_back(data.label, level);
      break;
    }

    for (auto &[d, child] : children) {
      if (auto r = child.match(method, next_path, vars); r != npos) {
        return r;
      }
    }

    int m = (uint32_t)methods;
    if (terminal && (methods & (1 << m))) {
      return handler_index[m];
    }

    return npos;
  }
};

int main() {

  // std::vector<std::function<void(match_entry *)>> handlers;

  // auto insert_route = [&](HttpMethod method, const char *route,
  //                         std::invocable<match_entry *> auto &&handler) {
  //   char *err = nullptr;
  //   void *index = reinterpret_cast<void *>(handlers.size());
  //   r3_tree_insert_routel_ex(tree, to_r3_method(method), route,
  //   strlen(route),
  //                            index, &err);
  //   if (err) {
  //     std::cerr << "Failed to insert route: " << err << std::endl;
  //   } else {
  //     handlers.emplace_back(std::forward<decltype(handler)>(handler));
  //   }
  // };

  // // initial capacity of variable buffer
  // size_t var_buf_capacity = 10;
  // void *var_buf = malloc(sizeof(r3_iovec_t) * var_buf_capacity);

  // auto dispatch_route = [&](HttpMethod method, std::string_view path) {
  //   // create entry without dynamic allocation
  //   // match_entry entry = {0};
  //   // entry.vars.tokens.entries = static_cast<r3_iovec_t *>(var_buf);
  //   // entry.vars.tokens.capacity = var_buf_capacity;
  //   // entry.vars.tokens.size = 0;
  //   // entry.path.base = path.data();
  //   // entry.path.len = path.length();

  //   auto entry = match_entry_createl(path.data(), path.length());
  //   entry->request_method = to_r3_method(method);
  //   auto matched = r3_tree_match_route(tree, entry);

  //   if (matched) {
  //     std::cout << "Matched route" << std::endl;
  //     size_t index = reinterpret_cast<size_t>(matched->data);
  //     handlers[index](entry);
  //   } else {
  //     std::cout << "Route failed to matched" << std::endl;
  //   }

  //   // incase capacity increased or changed
  //   // var_buf = entry.vars.tokens.entries;
  //   // var_buf_capacity = entry.vars.tokens.capacity;
  // };

  // // insert_route(HttpMethod::GET, "/api/posts",
  // //              [](auto) { std::cout << "GET posts" << std::endl; });

  // // insert_route(HttpMethod::POST, "/api/posts",
  // //              [](auto) { std::cout << "POST posts" << std::endl; });

  // // insert_route(HttpMethod::GET, "/api/users",
  // //              [](auto) { std::cout << "GET users" << std::endl; });

  // insert_route(HttpMethod::GET, "/api/profile/{id:\\d+}",
  //              [](match_entry *entry) {
  //                std::cout << "GET profile" << std::endl;
  //                std::cout << "id: " << entry->vars.tokens.entries <<
  //                std::endl;
  //              });

  // // dispatch_route(HttpMethod::GET, "/api/posts");
  // // dispatch_route(HttpMethod::POST, "/api/posts");
  // // dispatch_route(HttpMethod::GET, "/api/users");
  // dispatch_route(HttpMethod::GET, "/api/profile/69");

  // std::cout << NODE_COMPARE_OPCODE << std::endl;

  // free(var_buf);
  // r3_tree_free(tree);
}
