
#pragma once

#include "dbconnection.h"
#include "dbresult.h"
#include <functional>
#include <optional>
#include <string>

// part of public api
namespace hm {

class Stream;

// wrapper around Stream
class HttpResponse {
  friend class Stream;

public:
  void set_status(const char *status);
  void set_header(const char *name, const char *value);
  void set_header(const char *name, std::string_view value);
  void set_header_nc(const char *name, std::string_view value);

  void send(std::string &&str);
  void send_html(std::string &&str);
  void send_json(std::string &&str);
  void send_file(const char *path);

  void send_status_response(const char *status, std::string_view message);

  db::Connection get_db_connection();

private:
  HttpResponse(Stream *stream);

  Stream *stream_;
};

} // namespace hm
