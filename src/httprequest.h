#pragma once

#include <functional>
#include <optional>
#include <string>

#include "awaitabletask.h"
#include "httprouter.h"

#include "simdjson.h"

// part of public api
namespace hm {

class Stream;

// wrapper around Stream
class HttpRequest {
  friend class Stream;
  friend class HttpSession;
  friend class HttpRouter;

  using coro_handle =
      std::coroutine_handle<AwaitableTask<std::string_view>::Promise>;

public:
  std::optional<std::string_view> get_header(std::string_view header_name);
  std::string_view path();
  std::string_view query();

  HttpRequest &on_body(std::function<void(const std::string &)> &&cb);
  AwaitableTask<std::string_view> body();

  HttpRequest &on_data(std::function<void(std::string_view)> &&cb);

  AwaitableTask<std::string_view> data();

  AwaitableTask<simdjson::ondemand::document> json();

  constexpr std::optional<std::string_view>
  get_param(std::string_view label) const;

  std::optional<std::string_view> get_cookie(std::string_view key);

private:
  void add_to_body(std::string_view str);
  void handle_data(std::string_view str, bool eof = false);
  void handle_body();

private:
  HttpRequest(Stream *stream);

  const HttpRouter *router_;
  size_t handler_index_;

  Stream *stream_;

  std::string body_;
  std::string_view data_;

  std::function<void(const std::string &body)> on_body_cb_;
  std::function<void(std::string_view)> on_data_cb_;

  coro_handle body_coro_, data_coro_;
  bool data_chunk_mode_;
  bool data_ready_;
  bool buffered_;
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
