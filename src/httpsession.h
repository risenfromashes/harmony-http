#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <unordered_map>

#include <ev.h>
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>

#include "buffer.h"
#include "server.h"
#include "stream.h"
#include "util.h"
#include <atomic>

class HttpSession {

  using SSLSession = util::unique_ptr<SSL>;

  friend class Worker;
  friend class Stream;

public:
  HttpSession(Worker *worker, struct ev_loop *loop, int client_fd,
              SSLSession ssl);
  ~HttpSession();

  static SSLSession create_ssl_session(struct ssl_ctx_st *ssl_ctx, int fd);
  void remove_self();

  Stream *get_stream(int32_t id);
  void remove_stream(int32_t id);

  nghttp2_session *get_nghttp2_session();

  void start_settings_timer();
  void remove_settings_timer();

  static void fill_callback(nghttp2_session_callbacks *callbacks);

  Server *get_server();

private:
  static void settings_timeout_cb(struct ev_loop *loop, ev_timer *t,
                                  int revents);
  static void write_cb(struct ev_loop *loop, ev_io *t, int revents);
  static void read_cb(struct ev_loop *loop, ev_io *t, int revents);

  static ssize_t data_read_cb(nghttp2_session *session, int32_t stream_id,
                              uint8_t *buf, size_t length, uint32_t *data_flags,
                              nghttp2_data_source *source, void *user_data);

  int ssl_write(const uint8_t *data, size_t datalen);
  int ssl_read(uint8_t *data, size_t datalen);

  int verify_npn();
  int connection_made();

  int tls_handshake();
  int read();
  int write();

  inline int on_write() { return (this->*(this->write_func_))(); }

  inline int on_read() { return (this->*(this->read_func_))(); }

  int fill_wb();

  static int on_header_cb(nghttp2_session *session, const nghttp2_frame *frame,
                          nghttp2_rcbuf *name, nghttp2_rcbuf *value,
                          uint8_t flags, void *user_data);

  static int on_begin_headers_cb(nghttp2_session *session,
                                 const nghttp2_frame *frame, void *user_data);

  static int on_frame_recv_cb(nghttp2_session *session,
                              const nghttp2_frame *frame, void *user_data);

  static int on_frame_send_cb(nghttp2_session *session,
                              const nghttp2_frame *frame, void *user_data);

  static int send_data_cb(nghttp2_session *session, nghttp2_frame *frame,
                          const uint8_t *framehd, size_t length,
                          nghttp2_data_source *source, void *user_data);

  static int select_padding_cb(nghttp2_session *session,
                               const nghttp2_frame *frame, size_t max_payload,
                               void *user_data);

  static int on_data_chunk_recv_cb(nghttp2_session *session, uint8_t flags,
                                   int32_t stream_id, const uint8_t *data,
                                   size_t len, void *user_data);

  static int on_stream_close_cb(nghttp2_session *session, int32_t stream_id,
                                uint32_t error_code, void *user_data);

private:
  std::list<HttpSession>::iterator itr_;

  Worker *worker_;
  struct ev_loop *loop_;
  int client_fd_;

  ev_timer settings_timerev_;
  ev_io wev_;
  ev_io rev_;

  Buffer<64 * 1024> wbuf_;
  std::array<uint8_t, 16 * 1024> rbuf_;

  SSLSession ssl_;

  nghttp2_session *session_;

  int (HttpSession::*read_func_)();
  int (HttpSession::*write_func_)();

  const uint8_t *data_pending_ = nullptr;
  size_t data_pending_len_ = 0;

  std::unordered_map<int32_t, Stream> streams_;
};
