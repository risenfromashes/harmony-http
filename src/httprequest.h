#pragma once

#include <functional>
#include <optional>
#include <string>

// part of public api
namespace hm {

class Stream;

// wrapper around Stream
class HttpRequest {
  friend class Stream;
  friend class HttpSession;

  std::optional<std::string_view> get_header(std::string_view header_name);
  std::string_view path();
  std::string_view query();

  HttpRequest &on_body(std::function<void(const std::string &)> &&cb) {
    on_body_cb_ = std::move(cb);
    return *this;
  }

private:
  Stream *stream_;
  std::string body_;
  std::function<void(const std::string &body)> on_body_cb_;
};

} // namespace hm
