#include "httprequest.h"
#include "stream.h"

namespace hm {

HttpRequest::HttpRequest(Stream *stream)
    : stream_(stream), body_awaitable_(nullptr), data_awaitable_(nullptr),
      data_chunk_mode_(false) {}

std::optional<std::string_view>
HttpRequest::get_header(std::string_view header_name) {
  return stream_->headers.get_header(header_name);
}

std::string_view HttpRequest::path() { return stream_->path_; }

std::string_view HttpRequest::query() { return stream_->query_; }

HttpRequest &
HttpRequest::on_body(std::function<void(const std::string &)> &&cb) {
  data_chunk_mode_ = false;
  on_body_cb_ = std::move(cb);
  return *this;
}

HttpRequest &HttpRequest::on_data(std::function<void(std::string_view)> &&cb) {
  data_chunk_mode_ = true;
  on_data_cb_ = std::move(cb);
  return *this;
}

HttpRequest::DataAwaitable HttpRequest::body() {
  data_chunk_mode_ = false;
  return DataAwaitable(this, true);
}

HttpRequest::DataAwaitable HttpRequest::data() {
  data_chunk_mode_ = true;
  return DataAwaitable(this, false);
}
void HttpRequest::add_to_body(std::string_view str) { body_ += str; }

void HttpRequest::handle_data(std::string_view str) {
  if (data_chunk_mode_) {
    if (data_awaitable_) {
      data_awaitable_->resume(str);
    } else if (on_data_cb_) {
      on_data_cb_(body_);
    }
  } else {
    add_to_body(str);
  }
}

void HttpRequest::handle_body() {
  if (body_awaitable_) {
    body_awaitable_->resume(body_);
  } else if (on_body_cb_) {
    on_body_cb_(body_);
  }
}

}; // namespace hm
