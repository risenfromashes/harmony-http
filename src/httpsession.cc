#include <algorithm>
#include <cstdint>
#include <nghttp2/nghttp2.h>
#include <sys/socket.h>

#include <cstring>
#include <iostream>
#include <string_view>

#include "datastream.h"
#include "httpsession.h"
#include "server.h"
#include "util.h"
#include "worker.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

HttpSession::HttpSession(Worker *worker, struct ev_loop *loop, int client_fd,
                         SSLSession ssl)
    : worker_(worker), loop_(loop), client_fd_(client_fd), ssl_(std::move(ssl)),
      session_(nullptr) {

  ev_timer_init(&settings_timerev_, settings_timeout_cb, 10.0, 0.);
  settings_timerev_.data = this;

  ev_io_init(&rev_, read_cb, client_fd, EV_READ);
  ev_io_init(&wev_, write_cb, client_fd, EV_WRITE);
  wev_.data = this;
  rev_.data = this;

  ev_io_start(loop_, &rev_);

  SSL_set_accept_state(ssl_.get());
  read_func_ = &HttpSession::tls_handshake;
  write_func_ = &HttpSession::tls_handshake;

  streams_.reserve(10);
}

HttpSession::~HttpSession() {
  ev_timer_stop(loop_, &settings_timerev_);
  ev_io_stop(loop_, &rev_);
  ev_io_stop(loop_, &wev_);
  if (session_) {
    nghttp2_session_del(session_);
  }
  SSL_shutdown(ssl_.get());
  shutdown(client_fd_, SHUT_WR);
  close(client_fd_);
}

Server *HttpSession::get_server() { return worker_->server_; }

void HttpSession::settings_timeout_cb(struct ev_loop *loop, ev_timer *t,
                                      int revents) {
  auto *self = static_cast<HttpSession *>(t->data);

  std::cerr << "Terminating session due to settings timeout" << std::endl;

  if (self->session_) {
    nghttp2_session_terminate_session(self->session_, NGHTTP2_SETTINGS_TIMEOUT);
  }

  int rv = self->on_write();
  if (rv == -1) {
    self->remove_self();
  }
}

int HttpSession::ssl_write(const uint8_t *data, size_t datalen) {
  auto rv = SSL_write(ssl_.get(), data, datalen);

  if (rv <= 0) {
    auto err = SSL_get_error(ssl_.get(), rv);
    switch (err) {
    case SSL_ERROR_WANT_READ:
      // renegotiation started
      // disable renegotiation by default
      return -1;
    case SSL_ERROR_WANT_WRITE:
      ev_io_start(loop_, &wev_);
      return 0;
    default:
      std::cerr << "SSL write error: " << ERR_error_string(err, nullptr)
                << std::endl;
      return -1;
    }
  }

  return rv;
}

int HttpSession::ssl_read(uint8_t *buf, size_t buflen) {
  auto rv = SSL_read(ssl_.get(), buf, buflen);

  if (rv <= 0) {
    auto err = SSL_get_error(ssl_.get(), rv);
    switch (err) {
    case SSL_ERROR_WANT_READ:
      // try to write
      return on_write();
    case SSL_ERROR_WANT_WRITE:
      // disallow renegotiation
      return -1;
    default:
      return -1;
    }
  }

  return rv;
}

int HttpSession::fill_wb() {
  for (;;) {
    if (data_pending_) {
      auto n = wbuf_.write(data_pending_, data_pending_len_);
      if (n < data_pending_len_) {
        data_pending_len_ -= n;
        data_pending_ += n;
        return 0;
      }
      data_pending_ = nullptr;
      data_pending_len_ = 0;
    }

    const uint8_t *data;
    auto datalen = nghttp2_session_mem_send(session_, &data);

    if (datalen < 0) {
      std::cerr << "nghttp2_session_mem_send() returned error: "
                << nghttp2_strerror(datalen) << std::endl;
      return -1;
    }

    if (datalen == 0) {
      break;
    }

    data_pending_ = data;
    data_pending_len_ = datalen;
  }

  return 0;
}

void HttpSession::read_cb(struct ev_loop *loop, ev_io *w, int revents) {
  auto self = static_cast<HttpSession *>(w->data);

  int rv = self->on_read();
  if (rv == -1) {
    self->remove_self();
  }
}

void HttpSession::write_cb(struct ev_loop *loop, ev_io *w, int revents) {
  auto self = static_cast<HttpSession *>(w->data);

  int rv = self->on_write();
  if (rv == -1) {
    self->remove_self();
  }
}

void HttpSession::remove_self() { worker_->remove_session(this); }

HttpSession::SSLSession HttpSession::create_ssl_session(SSL_CTX *ssl_ctx,
                                                        int fd) {
  auto ssl = SSLSession(SSL_new(ssl_ctx), SSL_free);

  if (!ssl) {
    std::cerr << "SSL_new() failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  if (SSL_set_fd(ssl.get(), fd) == 0) {
    std::cerr << "SSL_set_fd() failed"
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  return ssl;
}

int HttpSession::verify_npn() {
  const unsigned char *next_proto = nullptr;
  unsigned int next_proto_len;
  SSL_get0_next_proto_negotiated(ssl_.get(), &next_proto, &next_proto_len);
  if (!next_proto) {
    SSL_get0_alpn_selected(ssl_.get(), &next_proto, &next_proto_len);
  }
  if (!next_proto) {
    return -1;
  }

  std::string_view proto(reinterpret_cast<const char *>(next_proto),
                         next_proto_len);

  if (util::check_h2_is_selected(proto)) {
    return 0;
  }

  return -1;
}

static int verbose_error_callback(nghttp2_session *session, int lib_error_code,
                                  const char *msg, size_t len,
                                  void *user_data) {
  std::cerr << "[nghttp2 error:" << lib_error_code << "] "
            << std::string_view(msg, len) << std::endl;
  return 0;
}

void HttpSession::fill_callback(nghttp2_session_callbacks *callbacks) {
  nghttp2_session_callbacks_set_on_stream_close_callback(
      callbacks, HttpSession::on_stream_close_cb);
  nghttp2_session_callbacks_set_on_frame_recv_callback(
      callbacks, HttpSession::on_frame_recv_cb);
  nghttp2_session_callbacks_set_on_frame_send_callback(
      callbacks, HttpSession::on_frame_send_cb);
  nghttp2_session_callbacks_set_error_callback2(callbacks,
                                                verbose_error_callback);
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
      callbacks, HttpSession::on_data_chunk_recv_cb);
  nghttp2_session_callbacks_set_on_header_callback2(callbacks,
                                                    HttpSession::on_header_cb);
  nghttp2_session_callbacks_set_on_begin_headers_callback(
      callbacks, HttpSession::on_begin_headers_cb);
  nghttp2_session_callbacks_set_send_data_callback(callbacks,
                                                   HttpSession::send_data_cb);
  /* no padding */
}

int HttpSession::connection_made() {

  int r = nghttp2_session_server_new(&session_, worker_->callbacks_, this);

  if (r != 0) {
    return r;
  }

  /* some config */
  nghttp2_settings_entry entries[1];
  size_t len = 1;
  entries[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  entries[0].value = 100; /* max concurrent streams */

  r = nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, entries, len);
  if (r != 0) {
    return r;
  }

  if (ssl_ && !util::check_http2_requirements(ssl_.get())) {
    nghttp2_session_terminate_session(session_, NGHTTP2_INADEQUATE_SECURITY);
    return -1;
  }

  return on_write();
}

int HttpSession::tls_handshake() {

  ev_io_stop(worker_->loop_, &wev_);

  ERR_clear_error();

  auto rv = SSL_do_handshake(ssl_.get());

  if (rv <= 0) {
    auto err = SSL_get_error(ssl_.get(), rv);
    switch (err) {
    case SSL_ERROR_WANT_READ:
      return 0;
    case SSL_ERROR_WANT_WRITE:
      ev_io_start(worker_->loop_, &wev_);
      return 0;
    default:
      std::cerr << "Error during SSL handshake [" << err
                << "] : " << ERR_error_string(err, nullptr) << std::endl;
      while ((err = ERR_get_error())) {
        std::cerr << "\t[Error:" << err << "] "
                  << ERR_error_string(err, nullptr) << std::endl;
      }
      return -1;
    }
  }

  if (verify_npn() != 0) {
    return -1;
  }

  read_func_ = &HttpSession::read;
  write_func_ = &HttpSession::write;

  if (connection_made() != 0) {
    return -1;
  }

  return 0;
}

int HttpSession::read() {

  // std::cerr << "Attempting read" << std::endl;

  ERR_clear_error();

  for (;;) {
    auto rv = ssl_read(rbuf_.data(), rbuf_.size());

    if (rv <= 0) {
      return rv;
    }

    auto nread = rv;
    rv = nghttp2_session_mem_recv(session_, rbuf_.data(), nread);

    if (rv < 0) {
      std::cerr << "nghttp2_session_mem_recv() returned error: "
                << nghttp2_strerror(rv) << std::endl;
      return -1;
    }
  }
  return 0;
}

int HttpSession::write() {
  ERR_clear_error();

  for (;;) {
    if (wbuf_.rleft() > 0) {
      auto rv = ssl_write(wbuf_.pos(), wbuf_.rleft());
      if (rv <= 0) {
        return rv;
      }
      wbuf_.drain(rv);
    } else {
      wbuf_.reset();
      if (fill_wb() != 0) {
        return -1;
      }
      if (wbuf_.rleft() == 0) {
        ev_io_stop(loop_, &wev_);
        break;
      }
    }
  }

  if (nghttp2_session_want_read(session_) == 0 &&
      nghttp2_session_want_write(session_) == 0 && wbuf_.rleft() == 0) {
    return -1;
  }

  return 0;
}

Stream *HttpSession::get_stream(int32_t id) {
  auto itr = streams_.find(id);
  if (itr == streams_.end()) {
    return nullptr;
  } else {
    return &(itr->second);
  }
}

void HttpSession::remove_stream(int32_t id) { streams_.erase(id); }

nghttp2_session *HttpSession::get_nghttp2_session() { return session_; }

void HttpSession::start_settings_timer() {
  ev_timer_start(loop_, &settings_timerev_);
}

void HttpSession::remove_settings_timer() {
  ev_timer_stop(loop_, &settings_timerev_);
}

ssize_t HttpSession::data_read_cb(nghttp2_session *session, int32_t stream_id,
                                  uint8_t *buf, size_t length,
                                  uint32_t *data_flags,
                                  nghttp2_data_source *source,
                                  void *user_data) {

  auto self = static_cast<HttpSession *>(user_data);
  auto stream = self->get_stream(stream_id);

  auto nread =
      std::min(stream->body_length_ - stream->body_offset_, (int64_t)length);

  *data_flags |= NGHTTP2_DATA_FLAG_NO_COPY;

  if (nread == 0 || stream->body_length_ == stream->body_offset_ + nread) {
    // end of stream
    // TODO: manage trailers
    *data_flags |= NGHTTP2_DATA_FLAG_EOF;

    if (nghttp2_session_get_stream_remote_close(self->get_nghttp2_session(),
                                                stream_id) == 0) {
      // stream should be half closed
      stream->stop_read_timeout();
      stream->stop_write_timeout();

      stream->submit_rst(NGHTTP2_NO_ERROR);
    }
  }

  return nread;
}

int HttpSession::on_header_cb(nghttp2_session *session,
                              const nghttp2_frame *frame, nghttp2_rcbuf *name,
                              nghttp2_rcbuf *value, uint8_t flags,
                              void *user_data) {

  auto self = static_cast<HttpSession *>(user_data);

  auto namebuf = nghttp2_rcbuf_get_buf(name);
  auto valuebuf = nghttp2_rcbuf_get_buf(value);

  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }

  auto stream = self->get_stream(frame->hd.stream_id);
  if (!stream) {
    return 0;
  }

  if (stream->headers.buffer_size + namebuf.len + valuebuf.len > 64 * 1024) {
    // too many headers
    // reject stream
    stream->submit_rst(NGHTTP2_INTERNAL_ERROR);
    return 0;
  }

  auto header_type = util::lookup_header(namebuf.base, namebuf.len);

  stream->headers.buffer_size += (namebuf.len + valuebuf.len);
  stream->headers.add_header(name, value);

  return 0;
}

int HttpSession::on_begin_headers_cb(nghttp2_session *session,
                                     const nghttp2_frame *frame,
                                     void *user_data) {

  auto self = static_cast<HttpSession *>(user_data);

  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }
  // add new stream
  auto [itr, inserted] = self->streams_.try_emplace(
      frame->hd.stream_id,
      /* arguments to stream ctor */ self, frame->hd.stream_id);

  if (!inserted) {
    // stream with same id exists
    return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
  }

  itr->second.reset_read_timeout();

  return 0;
}

int HttpSession::on_frame_recv_cb(nghttp2_session *session,
                                  const nghttp2_frame *frame, void *user_data) {

  auto self = static_cast<HttpSession *>(user_data);

  switch (frame->hd.type) {
  case NGHTTP2_DATA: {
    auto stream = self->get_stream(frame->hd.stream_id);
    if (!stream) {
      return 0;
    }

    // TODO: Handle POST/PUT: ON_DATA

    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      stream->stop_read_timeout();
      // TODO: Handle POST/PUT: ON_DATA_END
    } else {
      stream->reset_read_timeout();
    }
  }
  case NGHTTP2_HEADERS: {
    auto stream = self->get_stream(frame->hd.stream_id);
    if (!stream) {
      return 0;
    }

    if (frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
      // check for 100-continue
      auto expect = stream->headers.expect();
      if (expect.has_value() && util::streq_l("100-continue", expect.value())) {
        stream->submit_non_final_response("100");
      }

      auto method = stream->headers.method();
      if (method == "POST" || method == "PUT") {
        // TODO: Handle POST/PUT: ON_DATA_START
      }
    }

    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      stream->stop_read_timeout();
    } else {
      stream->reset_read_timeout();
    }

    // Respond after headers
    stream->prepare_response();
    break;
  }
  case NGHTTP2_SETTINGS: {
    if (frame->hd.flags & NGHTTP2_FLAG_ACK) {
      self->remove_settings_timer();
    }
  }
  defualt:
    break;
  }

  return 0;
}

int HttpSession::on_frame_send_cb(nghttp2_session *session,
                                  const nghttp2_frame *frame, void *user_data) {
  auto self = static_cast<HttpSession *>(user_data);

  switch (frame->hd.type) {
  case NGHTTP2_DATA:
  case NGHTTP2_HEADERS: {
    auto stream = self->get_stream(frame->hd.stream_id);

    if (!stream) {
      return 0;
    }

    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      stream->stop_write_timeout();
    } else if (std::min(nghttp2_session_get_stream_remote_window_size(
                            session, frame->hd.stream_id),
                        nghttp2_session_get_remote_window_size(session)) <= 0) {
      // idk what this checks (stream is blocked by flow control)
      stream->reset_read_timeout_if_active();
      stream->reset_write_timeout();
    } else {
      stream->reset_read_timeout_if_active();
      stream->stop_write_timeout();
    }

    break;
  }

  case NGHTTP2_SETTINGS: {
    if (frame->hd.flags & NGHTTP2_FLAG_ACK) {
      return 0;
    }
    // start setttings timer when setting frame is being sent
    self->start_settings_timer();
    break;
  }

  case NGHTTP2_PUSH_PROMISE: {
    auto promised_stream_id = frame->push_promise.promised_stream_id;
    auto promised_stream = self->get_stream(promised_stream_id);
    auto stream = self->get_stream(frame->hd.stream_id);

    if (!stream || !promised_stream) {
      return 0;
    }

    stream->reset_read_timeout_if_active();
    stream->reset_write_timeout();

    stream->prepare_response();
    break;
  }
  }
  return 0;
}

int HttpSession::send_data_cb(nghttp2_session *session, nghttp2_frame *frame,
                              const uint8_t *framehd, size_t length,
                              nghttp2_data_source *source, void *user_data) {
  auto http_session = static_cast<HttpSession *>(user_data);
  auto stream = http_session->get_stream(frame->hd.stream_id);
  auto &wb = http_session->wbuf_;
  auto padlen = frame->data.padlen;
  auto self = static_cast<DataStream *>(source->ptr);

  if (wb.wleft() < 9 + length + padlen) {
    return NGHTTP2_ERR_WOULDBLOCK;
  }

  wb.write_full(framehd, 9);

  if (padlen) {
    wb.write_byte(padlen - 1);
  }

  int rv = self->send_data(self, stream, length);
  if (rv != 0) {
    return rv;
  }

  stream->body_offset_ += length;

  if (padlen) {
    wb.fill(0, padlen - 1);
  }

  return 0;
}

int HttpSession::select_padding_cb(nghttp2_session *session,
                                   const nghttp2_frame *frame,
                                   size_t max_payload, void *user_data) {
  auto self = static_cast<HttpSession *>(user_data);
  return std::min(max_payload, frame->hd.length + /* padding */ 0);
}

int HttpSession::on_data_chunk_recv_cb(nghttp2_session *session, uint8_t flags,
                                       int32_t stream_id, const uint8_t *data,
                                       size_t len, void *user_data) {

  auto self = static_cast<HttpSession *>(user_data);
  auto stream = self->get_stream(stream_id);

  if (!stream) {
    return 0;
  }

  // TODO: Handle post
  stream->reset_read_timeout();
  return 0;
}

int HttpSession::on_stream_close_cb(nghttp2_session *session, int32_t stream_id,
                                    uint32_t error_code, void *user_data) {
  auto self = static_cast<HttpSession *>(user_data);
  self->remove_stream(stream_id);
  return 0;
}
