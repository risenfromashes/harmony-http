#include "httpresponse.h"
#include "stream.h"
#include "stringstream.h"
#include "worker.h"

namespace hm {
HttpResponse::HttpResponse(Stream *stream) : stream_(stream) {}

void HttpResponse::set_status(const char *status) {
  stream_->response_headers.status = status;
}

void HttpResponse::set_header(const char *name, const char *value) {
  stream_->response_headers.set_header(name, value);
}

void HttpResponse::set_header(const char *name, std::string_view value) {
  stream_->response_headers.set_header(name, value);
}
void HttpResponse::set_header_nc(const char *name, std::string_view value) {
  stream_->response_headers.set_header_nc(name, value);
}

void HttpResponse::send(std::string &&str) {
  stream_->submit_string_response(std::move(str));
}

void HttpResponse::send_html(std::string &&str) {
  stream_->submit_html_response(std::move(str));
}

void HttpResponse::send_json(std::string &&str) {
  stream_->submit_json_response(std::move(str));
}

void HttpResponse::send_file(const char *path) {
  if (path == nullptr) {
    stream_->submit_file_response();
  } else {
    stream_->submit_file_response(path);
  }
}

db::Connection HttpResponse::get_db_connection() { return stream_; }

void HttpResponse::send_status_response(const char *status,
                                        std::string_view message) {
  stream_->response_headers.status = status;
  stream_->submit_html_response("<html> <h1>" + std::string(status) +
                                "</h1> <p>" + std::string(message) +
                                "</p> </html>");
}
} // namespace hm
