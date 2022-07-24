#include "httpresponse.h"
#include "stream.h"
#include "stringstream.h"

namespace hm {

void HttpResponse::set_status(const char *status) {
  stream_->response_headers.status = status;
}

void HttpResponse::set_header(const char *name, const char *value) {
  stream_->response_headers.set_header(name, value);
}

void HttpResponse::set_header(const char *name, std::string &&value) {
  stream_->response_headers.set_header(name, value);
}
void HttpResponse::set_header_nc(const char *name, std::string_view value) {
  stream_->response_headers.set_header_nc(name, value);
}

void HttpResponse::send(std::string &&str) {
  stream_->add_data_stream<StringStream>(std::move(str));
}

void HttpResponse::send_file(const char *path) {
  auto fs = stream_->get_static_file(path, true);
  if (fs) {
    stream_->add_data_stream(fs);
  }
}

} // namespace hm
