#include "httprequest.h"
#include "coro.h"
#include "httpsession.h"
#include "worker.h"

namespace hm {

HttpRequest::HttpRequest(Stream *stream)
    : stream_(stream), data_chunk_mode_(false), data_ready_(false),
      buffered_(false) {
  data_coro_ = body_coro_ = nullptr;
}

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
  if (buffered_) {
    on_data_cb_(body_);
    buffered_ = false;
  }
  return *this;
}

AwaitableTask<std::string_view> HttpRequest::body() {
  data_chunk_mode_ = false;
  body_coro_ = coro_handle::from_address((co_await this_coro()).address());
  if (!data_ready_) {
    co_await std::suspend_always{};
  }
  co_return body_;
}

AwaitableTask<std::string_view> HttpRequest::data() {
  data_chunk_mode_ = true;
  data_coro_ = coro_handle::from_address((co_await this_coro()).address());
  // data may be buffered before coroutine/callback is registered
  if (buffered_) {
    buffered_ = false;
    co_return body_;
  } else {
    body_.clear();
    if (data_ready_) {
      // body_ was previously returned and cleared
      // hence we returned data
      // return eof now
      co_return std::string_view();
    } else {
      co_await std::suspend_always{};
      co_return data_;
    }
  }
}

void HttpRequest::add_to_body(std::string_view str) { body_ += str; }

void HttpRequest::handle_data(std::string_view str, bool eof) {
  // ignore empty string, empty string is used to signal eof
  if (!str.empty() || eof) {
    assert(!eof || str.empty());

    if (data_chunk_mode_) {
      data_ = str;

      if (data_coro_) {
        data_coro_.resume();
        return;
      } else if (on_data_cb_) {
        on_data_cb_(data_);
        return;
      }
    }

    buffered_ = true;
    add_to_body(str);
  }
}

void HttpRequest::handle_body() {
  // end of data stream
  if (data_chunk_mode_) {
    handle_data(std::string_view(), true);
  } else {
    if (body_coro_) {
      body_coro_.resume();
    } else if (on_body_cb_) {
      on_body_cb_(body_);
    } else {
      data_ready_ = true;
      // data is ready, no one handled it yet
    }
  }
}

AwaitableTask<simdjson::ondemand::document> HttpRequest::json() {
  co_await body();
  body_.reserve(body_.size() + simdjson::SIMDJSON_PADDING);
  auto parser = Worker::get_worker()->get_json_parser();
  co_return parser->iterate(body_);
}

std::optional<std::string_view> HttpRequest::get_cookie(std::string_view key) {
  auto cookies = get_header("cookie");
  if (!cookies) {
    return std::nullopt;
  }
  return util::get_cookie(cookies.value(), key);
}

}; // namespace hm
