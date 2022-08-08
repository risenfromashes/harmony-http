
#include "httprouter.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "stream.h"

namespace hm {

HttpMethod method_from_string(std::string_view str) {
  using enum HttpMethod;
  if (util::streq_l(str, "GET")) {
    return GET;
  } else if (util::streq_l(str, "POST")) {
    return POST;
  } else if (util::streq_l(str, "PUT")) {
    return PUT;
  } else if (util::streq_l(str, "HEAD")) {
    return HEAD;
  } else if (util::streq_l(str, "PATCH")) {
    return PATCH;
  } else if (util::streq_l(str, "DELETE")) {
    return DELETE;
  } else if (util::streq_l(str, "OPTIONS")) {
    return OPTIONS;
  }
  return GET;
}

RouteNode::RouteNode() {
  data.type = RouteNodeData::Type::ROOT;
  terminal = false;
}

RouteNode::RouteNode(const RouteNodeData &data) : data(data) {}

RouteNode::RouteNode(RouteNode &&b)
    : data(b.data), terminal(b.terminal), methods(b.methods),
      children(std::move(b.children)) {
  std::copy(b.route_index, b.route_index + 8, route_index);
}

RouteNode &RouteNode::operator=(RouteNode &&b) {
  data = b.data;
  terminal = b.terminal;
  methods = b.methods;
  children = std::move(b.children);
  std::copy(b.route_index, b.route_index + 8, route_index);
  return *this;
}

void RouteNode::insert_path(HttpMethod method, std::string_view path,
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
std::size_t RouteNode::match(HttpMethod method, std::string_view path,
                             std::vector<std::string_view> &vars) const {

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

bool HttpRouter::dispatch_route(HttpMethod method, std::string_view path,
                                HttpRequest *request,
                                HttpResponse *response) const {
  get_vars_vector().clear();
  auto index = root_.match(method, path, get_vars_vector());
  if (index != RouteNode::npos) {
    request->router_ = this;
    request->handler_index_ = index;
    if (std::holds_alternative<coro_func>(handlers_[index])) {
      auto &h = std::get<coro_func>(handlers_[index]);
      request->stream_->coro_handler_ = std::invoke(h, request, response);
    } else {
      auto &h = std::get<func>(handlers_[index]);
      std::invoke(h, request, response);
    }
    return true;
  }
  return false;
}

} // namespace hm
