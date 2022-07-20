
#pragma once

#include <functional>
#include <optional>
#include <string>

// part of public api
namespace hm {

class Stream;

// wrapper around Stream
class HttpResponse {
  friend class Stream;

  void set_header(const char *name, const char *value);
  void set_header(const char *name, std::string &&value);
  void set_header_nc(const char *name, std::string_view value);

private:
  Stream *stream_;
  std::function<void(const std::string &body)> on_body_cb_;
};
} // namespace hm
