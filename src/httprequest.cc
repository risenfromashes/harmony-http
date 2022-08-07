#include "httprequest.h"
#include "coro.h"
#include "stream.h"

namespace hm {

HttpRequest::HttpRequest(Stream *stream)
    : stream_(stream), data_chunk_mode_(false) {}

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

AwaitableTask<std::string_view> HttpRequest::body() {
  data_chunk_mode_ = false;
  body_coro_ = coro_handle::from_address((co_await this_coro()).address());
  co_await std::suspend_always{};
  co_return body_;
}

AwaitableTask<std::string_view> HttpRequest::data() {
  data_chunk_mode_ = true;
  data_coro_ = coro_handle::from_address((co_await this_coro()).address());
  co_await std::suspend_always{};
  co_return body_;
}

void HttpRequest::add_to_body(std::string_view str) { body_ += str; }

void HttpRequest::handle_data(std::string_view str) {
  if (data_chunk_mode_) {
    if (data_coro_) {
      body_ = str;
      data_coro_.resume();
    } else if (on_data_cb_) {
      on_data_cb_(body_);
    }
  } else {
    add_to_body(str);
  }
}

void HttpRequest::handle_body() {
  if (body_coro_) {
    body_coro_.resume();
  } else if (on_body_cb_) {
    on_body_cb_(body_);
  }
}

}; // namespace hm
