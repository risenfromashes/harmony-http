#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include <ev.h>
#include <nghttp2/nghttp2.h>

#include "buffer.h"
#include "datastream.h"
#include "dbsession.h"
#include "filestream.h"
#include "stringstream.h"
#include "util.h"

namespace hm {

class HttpSession;

class Stream {

  friend class Worker;
  friend class HttpSession;
  friend class StringStream;
  friend class FileStream;
  friend class HttpRequest;

public:
  Stream(HttpSession *session, int32_t stream_id);
  ~Stream();

  int32_t id() { return id_; }
  uint64_t serial() { return serial_; }

  HttpSession *get_session();
  db::Session *get_db_session();

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

  /* int submit_data_response(...) */
  /* int submit_push_promise(...) */
  int submit_response(std::string_view status, DataStream *stream);
  int submit_rst(uint32_t error_code);
  int submit_non_final_response(std::string_view status);

  int submit_html_response(std::string_view status, std::string_view response);
  int submit_json_response(std::string_view status, std::string &&response);
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
    std::optional<std::string_view> method() {
      return get_string_view(rcbuf.method);
    }
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

    struct RCBuf {
      using rcbuf_pair = std::pair<nghttp2_rcbuf *, nghttp2_rcbuf *>;
      nghttp2_rcbuf *method;
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

    uint32_t unindexed_headers_len = 0;
    size_t buffer_size = 0;

    std::optional<std::string_view> get_header(std::string_view header_name);
    void add_header(nghttp2_rcbuf *name, nghttp2_rcbuf *value);

  } headers;

  constexpr static size_t reserved_response_headers_len = 1;

  struct ResponseHeader {
    inline constexpr static uint32_t max_nva_len = 20;

    std::array<nghttp2_nv, max_nva_len> nva;

    size_t nvlen = reserved_response_headers_len;
    // std::vector<nghttp2_nv> additional;

    /* name is string literal, value will be copied */
    void set_header(const char *name, std::string_view value) {
      nva[nvlen++] = util::make_nv(name, value, {false, true});
    }
    /* no copy */
    void set_header_nc(const char *name, std::string_view value) {
      nva[nvlen++] = util::make_nv(name, value, {false, false});
    }
  } response_headers;

private:
  /* unique identifier of streams per thread */
  uint64_t serial_;

  HttpSession *session_;

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
};
} // namespace hm
