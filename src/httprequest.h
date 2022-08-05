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

  struct DataAwaitable : public Awaitable<> {
    handle_type handle;
    std::string_view str;
    void await_suspend(handle_type handle) { this->handle = handle; }
    std::string_view await_resume() { return str; }
    void resume(std::string_view str) { this->str = str, handle.resume(); }
    DataAwaitable(HttpRequest *request, bool body) {
      if (body) {
        request->body_awaitable_ = this;
      } else {
        request->data_awaitable_ = this;
      }
    }
  };

  std::optional<std::string_view> get_header(std::string_view header_name);
  std::string_view path();
  std::string_view query();

  HttpRequest &on_body(std::function<void(const std::string &)> &&cb);
  DataAwaitable body();

  HttpRequest &on_data(std::function<void(std::string_view)> &&cb);
  DataAwaitable data();

  constexpr std::optional<std::string_view>
  get_param(std::string_view label) const;

private:
  void add_to_body(std::string_view str);
  void handle_data(std::string_view str);
  void handle_body();

private:
  HttpRequest(Stream *stream);

  const HttpRouter *router_;
  size_t handler_index_;

  Stream *stream_;
  std::string body_;
  std::function<void(const std::string &body)> on_body_cb_;
  std::function<void(std::string_view)> on_data_cb_;

  DataAwaitable *body_awaitable_, *data_awaitable_;
  bool data_chunk_mode_;
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
