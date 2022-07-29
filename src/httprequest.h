#pragma once

#include <functional>
#include <optional>
#include <string>

#include "httprouter.h"

// part of public api
namespace hm {

class Stream;

// wrapper around Stream
class HttpRequest {
  friend class Stream;
  friend class HttpSession;
  friend class HttpRouter;

  std::optional<std::string_view> get_header(std::string_view header_name);
  std::string_view path();
  std::string_view query();

  HttpRequest &on_body(std::function<void(const std::string &)> &&cb) {
    on_body_cb_ = std::move(cb);
    return *this;
  }

  HttpRequest &on_data(std::function<void(std::string_view)> &&cb) {
    on_data_cb_ = std::move(cb);
    return *this;
  }

  constexpr std::optional<std::string_view>
  get_param(std::string_view label) const;

private:
  HttpRequest(Stream *stream);

  const HttpRouter *router_;
  size_t handler_index_;

  Stream *stream_;
  std::string body_;
  std::function<void(const std::string &body)> on_body_cb_;
  std::function<void(std::string_view)> on_data_cb_;
};

constexpr std::optional<std::string_view>
HttpRequest::get_param(std::string_view label) const {
  auto route = std::string_view(router_->route_paths_[handler_index_]);
  // goto bracket, match label, repeat
  size_t pos, index = 0;
  while ((pos = route.find('{')) != route.npos) {
    if (route.substr(pos + 1, label.size()) == label) {
      return router_->get_vars_vector()[index];
    }
    route = route.substr(pos + 1);
    index++;
  }
  return std::nullopt;
}

} // namespace hm
