#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <variant>

#include <ev.h>
#include <nghttp2/nghttp2.h>

#include "buffer.h"
#include "datastream.h"
#include "dbsession.h"
#include "filestream.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "httprouter.h"
#include "stringstream.h"
#include "util.h"

namespace hm {

class HttpSession;
class UUIDGenerator;

class Stream {

  friend class Worker;
  friend class HttpSession;
  friend class StringStream;
  friend class FileStream;
  friend class HttpRequest;
  friend class HttpRouter;

public:
  using string_view_pair = std::pair<std::string_view, std::string_view>;

  Stream(HttpSession *session, int32_t stream_id);
  ~Stream();

  int32_t id() { return id_; }
  uint64_t serial() { return serial_; }

  HttpSession *get_session();
  db::Session *get_db_session();
  UUIDGenerator *get_uuid_generator();

  Buffer<64 * 1024> *get_buffer();
  // resets read timeout
  void reset_read_timeout();
  // resets write timeout
  void reset_write_timeout();
  // resets read timeout if timeout active (not stopped or timed out)
  void reset_read_timeout_if_active();
  // resets read timeout if timeout active (not stopped or timed out)
  void reset_write_timeout_if_active();
  // stops timeout
  void stop_read_timeout();
  // stops timeout
  void stop_write_timeout();

  static void timeout_cb(struct ev_loop *loop, ev_timer *w, int revents);

  /* own the DataStream */
  template <std::derived_from<DataStream> T>
  T *add_data_stream(auto &&...args) {
    auto &ds =
        data_stream_store_.emplace<T>(std::forward<decltype(args)>(args)...);
    body_length_ = ds.length();
    body_offset_ = 0;
    data_stream_ = &ds;
    return &ds;
  }

  /* don't own the DataStream */
  template <std::derived_from<DataStream> T> T *add_data_stream(T *ds) {
    body_length_ = ds->length();
    body_offset_ = 0;
    data_stream_ = ds;
    return ds;
  }

  FileStream *get_static_file(const std::string_view &rel_path,
                              bool prefer_compressed = true);

  void parse_path();

  /* int submit_data_response(...) */
  /* int submit_push_promise(...) */
  int submit_rst(uint32_t error_code);
  int submit_non_final_response(std::string_view status);

  int submit_response(DataStream *stream);
  // int submit_string_response(std::string_view status,
  //                            std::initializer_list<string_view_pair> headers,
  //                            std::string_view response);
  int submit_string_response(std::string &&str);
  int submit_html_response(std::string &&response);
  int submit_json_response(std::string &&response);
  int submit_file_response(std::string_view path);
  int submit_file_response();

  /* void prepare_status_response(...) */
  /* void prepare_redirect_response(...) */
  int prepare_response();

  struct Headers {

  private:
    std::optional<std::string_view> get_string_view(nghttp2_rcbuf *rcbuf) {
      if (rcbuf) {
        return util::to_string_view(rcbuf);
      }
      return std::nullopt;
    }

  public:
    std::optional<std::string_view> scheme() {
      return get_string_view(rcbuf.scheme);
    }
    std::optional<std::string_view> authority() {
      return get_string_view(rcbuf.authority);
    }
    std::optional<std::string_view> host() {
      return get_string_view(rcbuf.host);
    }
    std::optional<std::string_view> path() {
      return get_string_view(rcbuf.path);
    }
    std::optional<std::string_view> ims() {
      return get_string_view(rcbuf.ims /* */);
    }
    std::optional<std::string_view> expect() {
      return get_string_view(rcbuf.expect);
    }

    inline constexpr static uint32_t max_nva_len = 10;

    HttpMethod method;
    struct RCBuf {
      using rcbuf_pair = std::pair<nghttp2_rcbuf *, nghttp2_rcbuf *>;
      nghttp2_rcbuf *scheme;
      nghttp2_rcbuf *authority;
      nghttp2_rcbuf *host;
      nghttp2_rcbuf *path;
      nghttp2_rcbuf *expect;
      nghttp2_rcbuf *ims;
      std::array<rcbuf_pair, max_nva_len> nva;
      size_t nvlen;
      std::vector<rcbuf_pair> additional;

      RCBuf();
      ~RCBuf();
    } rcbuf;

    size_t buffer_size = 0;

    std::optional<std::string_view> get_header(std::string_view header_name);
    void add_header(nghttp2_rcbuf *name, nghttp2_rcbuf *value);

  } headers;

  constexpr static size_t reserved_response_headers_len = 1;

  struct ResponseHeader {
    const char *status;
    inline constexpr static uint32_t max_nva_len = 10;
    std::array<nghttp2_nv, max_nva_len> nva;
    std::vector<nghttp2_nv> nvector;

    size_t nvlen = reserved_response_headers_len;
    // std::vector<nghttp2_nv> additional;

    /* name is string literal, value will be copied */
    void push_header(const nghttp2_nv &nv) {
      if (nvlen < max_nva_len) {
        nva[nvlen] = nv;
      } else {
        if (nvlen == max_nva_len) {
          std::move(nva.begin(), nva.end(), std::back_inserter(nvector));
        }
        nvector.push_back(nv);
      }
      nvlen++;
    }

    void set_header(const char *name, const char *value) {
      push_header(util::make_nv(name, value, {false, false}));
    }

    void set_header(const char *name, std::string_view value) {
      push_header(util::make_nv(name, value, {false, true}));
    }
    /* no copy */
    void set_header_nc(const char *name, std::string_view value) {
      push_header(util::make_nv(name, value, {false, false}));
    }

    /* to set :status header at the beginning of the buffer */
    void set_status() {
      auto nv = util::make_nv(":status", status, {false, false});
      if (nvlen <= max_nva_len) {
        nva[0] = nv;
      } else {
        nvector[0] = nv;
      }
    }

    std::pair<nghttp2_nv *, size_t> get_buffer() {
      if (nvlen <= max_nva_len) {
        return {nva.data(), nvlen};
      } else {
        return {nvector.data(), nvlen};
      }
    }

    ResponseHeader() : status(nullptr) {}
  } response_headers;

private:
  /* unique identifier of streams per thread */
  uint64_t serial_;

  HttpSession *session_;

  HttpRequest request_;
  HttpResponse response_;

  std::string_view path_;
  std::string_view query_;

  ev_timer rtimer_;
  ev_timer wtimer_;

  int64_t body_length_;
  int64_t body_offset_;

  int32_t id_;

  util::MemBlock<512> mem_block_;

  std::variant<std::monostate, StringStream, FileStream> data_stream_store_;
  DataStream *data_stream_ = nullptr;

  Task<> coro_handler_;

  bool prepared_response_ = false;
};

} // namespace hm
