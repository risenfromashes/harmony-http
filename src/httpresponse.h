
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

  void set_status(const char *status);
  void set_header(const char *name, const char *value);
  void set_header(const char *name, std::string &&value);
  void set_header_nc(const char *name, std::string_view value);

  void send(std::string &&str);
  void send_file(const char *path);

private:
  HttpResponse(Stream *stream);

  Stream *stream_;
};

// void send_query(HttpResponse *res, const char *command,
//                 std::function<void(PGresult *result)> &&on_sucess = {},
//                 std::function<void(PGresult *result)> &&on_error = {});

// void send_query_params(Stream *stream, const char *command,
//                        std::initializer_list<std::string> params,
//                        std::function<void(PGresult *result)> &&on_sucess =
//                        {}, std::function<void(PGresult *result)> &&on_error =
//                        {});
} // namespace hm
