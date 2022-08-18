#include <ev.h>
#include <iostream>

#include "buffer.h"
#include "httpsession.h"
#include "stream.h"
#include "util.h"
#include "worker.h"

#include <nghttp2/nghttp2.h>

#include <postgresql/libpq-fe.h>
#include <string_view>
#include <variant>

namespace hm {

Stream::Stream(HttpSession *session, int32_t stream_id)
    : headers{}, session_(session), request_(this), response_(this),
      id_(stream_id) {
  ev_timer_init(&rtimer_, timeout_cb, 0., 30.);
  ev_timer_init(&wtimer_, timeout_cb, 0., 30.);
  rtimer_.data = this;
  wtimer_.data = this;
  session_->worker_->add_stream(this);
}

Stream::~Stream() {
  session_->worker_->remove_stream(this);
  ev_timer_stop(session_->loop_, &rtimer_);
  ev_timer_stop(session_->loop_, &wtimer_);
}

Stream::Headers::RCBuf::RCBuf() {
  scheme = authority = host = path = expect = ims = nullptr;
  nvlen = 0;
}

Stream::Headers::RCBuf::~RCBuf() {
  if (scheme) {
    nghttp2_rcbuf_decref(scheme);
  }
  if (authority) {
    nghttp2_rcbuf_decref(authority);
  }
  if (host) {
    nghttp2_rcbuf_decref(host);
  }
  if (path) {
    nghttp2_rcbuf_decref(path);
  }
  if (expect) {
    nghttp2_rcbuf_decref(expect);
  }
  if (ims) {
    nghttp2_rcbuf_decref(ims);
  }
  for (int i = 0; i < nvlen; i++) {
    nghttp2_rcbuf_decref(nva[i].first);
    nghttp2_rcbuf_decref(nva[i].second);
  }
  if (nvlen == max_nva_len) {
    for (int i = 0; i < additional.size(); i++) {
      nghttp2_rcbuf_decref(additional[i].first);
      nghttp2_rcbuf_decref(additional[i].second);
    }
  }
}

std::optional<std::string_view>
Stream::Headers::get_header(std::string_view header_name) {
  auto header_t = util::lookup_header(header_name);
  if (util::streq_l(header_name, "expect")) {
    return expect();
  } else if (util::streq_l(header_name, "if-modified-since")) {
    return ims();
  } else {
    for (int i = 0; i < rcbuf.nvlen; i++) {
      if (util::streq_l(header_name,
                        get_string_view(rcbuf.nva[i].first).value())) {
        return get_string_view(rcbuf.nva[i].second);
      }
    }
    if (rcbuf.nvlen == max_nva_len) {
      for (int i = 0; i < rcbuf.additional.size(); i++) {
        if (util::streq_l(header_name,
                          get_string_view(rcbuf.additional[i].first).value())) {
          return get_string_view(rcbuf.additional[i].second);
        }
      }
    }
  }
  return std::nullopt;
}

void Stream::Headers::add_header(nghttp2_rcbuf *name, nghttp2_rcbuf *value) {
  auto namebuf = nghttp2_rcbuf_get_buf(name);
  auto header_t = util::lookup_header(namebuf.base, namebuf.len);

  nghttp2_rcbuf_incref(value);
  switch (header_t) {
  case util::HttpHeader::pMETHOD:
    method = hm::method_from_string(get_string_view(value).value());
    nghttp2_rcbuf_decref(value);
    break;
  case util::HttpHeader::pSCHEME:
    rcbuf.scheme = value;
    break;
  case util::HttpHeader::pAUTHORITY:
    rcbuf.authority = value;
    break;
  case util::HttpHeader::pHOST:
    rcbuf.host = value;
    break;
  case util::HttpHeader::pPATH:
    rcbuf.path = value;
    break;
  case util::HttpHeader::EXPECT:
    rcbuf.expect = value;
    break;
  case util::HttpHeader::IF_MODIFIED_SINCE:
    rcbuf.ims = value;
    break;
  default:
    nghttp2_rcbuf_incref(name);
    if (rcbuf.nvlen < max_nva_len) {
      rcbuf.nva[rcbuf.nvlen++] = {name, value};
    } else {
      rcbuf.additional.emplace_back(name, value);
    }
    break;
  }
}

HttpSession *Stream::get_session() { return session_; }

db::Session *Stream::get_db_session() {
  return session_->worker_->get_db_session();
}

UUIDGenerator *Stream::get_uuid_generator() {
  return session_->worker_->get_uuid_generator();
}

FileEntry *Stream::get_static_file(const std::string_view &rel_path,
                                   bool prefer_compressed) {
  return session_->worker_->get_static_file(rel_path, prefer_compressed);
}

Buffer<64 * 1024> *Stream::get_buffer() { return &session_->wbuf_; }

void Stream::reset_read_timeout() { ev_timer_again(session_->loop_, &rtimer_); }

void Stream::reset_write_timeout() {
  ev_timer_again(session_->loop_, &wtimer_);
}

void Stream::reset_read_timeout_if_active() {
  if (ev_is_active(&rtimer_)) {
    ev_timer_again(session_->loop_, &rtimer_);
  }
}

void Stream::reset_write_timeout_if_active() {
  if (ev_is_active(&wtimer_)) {
    ev_timer_again(session_->loop_, &wtimer_);
  }
}

void Stream::stop_read_timeout() { ev_timer_stop(session_->loop_, &rtimer_); }

void Stream::stop_write_timeout() { ev_timer_stop(session_->loop_, &wtimer_); }

void Stream::timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto self = static_cast<Stream *>(w->data);
  auto session = self->session_;

  std::cerr << "Ending stream " << self->id_ << " due to read/write timeout"
            << std::endl;

  ev_timer_stop(session->loop_, &self->rtimer_);
  ev_timer_stop(session->loop_, &self->wtimer_);

  self->submit_rst(NGHTTP2_INTERNAL_ERROR);

  int rv = session->on_write();
  if (rv == -1) {
    session->remove_self();
  }
}

int Stream::submit_response(DataStream *data_stream) {
  int rv;
  if (response_headers.status == nullptr) {
    response_headers.status = "200";
  }

  response_headers.set_status();

  auto [nvbuf, nvlen] = response_headers.get_buffer();

  nghttp2_data_provider dp;
  if (data_stream) {
    dp.source.ptr = data_stream;
    dp.read_callback = &HttpSession::data_read_cb;
    rv = nghttp2_submit_response(session_->get_nghttp2_session(), id_, nvbuf,
                                 nvlen, &dp);
  } else {
    rv = nghttp2_submit_response(session_->get_nghttp2_session(), id_, nvbuf,
                                 nvlen, nullptr);
  }
  ev_io_start(session_->worker_->loop_, &session_->wev_);
  return rv;
}

int Stream::submit_rst(uint32_t error_code) {
  stop_read_timeout();
  stop_write_timeout();

  int rv = nghttp2_submit_rst_stream(session_->get_nghttp2_session(),
                                     NGHTTP2_FLAG_NONE, id_, error_code);
  ev_io_start(session_->worker_->loop_, &session_->wev_);
  return rv;
}

int Stream::submit_non_final_response(std::string_view status) {
  nghttp2_nv nva[] = {util::make_nv(":status", status)};
  int rv =
      nghttp2_submit_headers(session_->get_nghttp2_session(), NGHTTP2_FLAG_NONE,
                             id_, nullptr, nva, 1, nullptr);
  ev_io_start(session_->worker_->loop_, &session_->wev_);
  return rv;
}

int Stream::submit_string_response(std::string &&response) {
  auto ss = add_data_stream<StringStream>(std::move(response));

  response_headers.set_header_nc("content-length",
                                 util::to_string(ss->length(), mem_block_));

  return submit_response(ss);
}

int Stream::submit_html_response(std::string &&response) {

  auto ss = add_data_stream<StringStream>(std::move(response));

  response_headers.set_header_nc("content-type", "text/html; charset=utf-8");
  response_headers.set_header_nc("content-length",
                                 util::to_string(ss->length(), mem_block_));

  return submit_response(ss);
}

int Stream::submit_json_response(std::string &&response) {

  auto ss = add_data_stream<StringStream>(std::move(response));

  response_headers.set_header_nc("content-type", "application/json");
  response_headers.set_header_nc("content-length",
                                 util::to_string(ss->length(), mem_block_));

  return submit_response(ss);
}

int Stream::submit_file_response(std::string_view path) {

  auto file = get_static_file(path, true);

  if (file) {

    time_t last_mod = 0;
    bool last_mod_found = false;
    if (headers.ims()) {
      /* nghttp2 rcbuf is value is guaranteed to be null terminated */
      last_mod = util::parse_http_date(headers.ims().value().data());
      last_mod_found = true;
    }

    auto fs = add_data_stream<FileStream>(file);

    auto [mtime, length] = file->info();
    auto date = session_->get_cached_date();

    if (last_mod_found && mtime <= last_mod) {
      response_headers.set_header_nc("date", date);
      response_headers.status = "304";
      return submit_response(nullptr);
    } else {
      response_headers.set_header_nc("content-type", file->mime_type());
      response_headers.set_header_nc("content-length",
                                     util::to_string(length, mem_block_));
      if (file->compressed()) {
        response_headers.set_header_nc("content-encoding", file->encoding());
      }
      /* for development, files might change */
      response_headers.set_header_nc("cache-control", "max-age=0");
      response_headers.set_header_nc("date", date);
      response_headers.set_header_nc("last-modified",
                                     util::http_date(mtime, mem_block_));
      return submit_response(fs);
    }
  } else {
    response_headers.status = "404";
    submit_json_response("<html><h1>404</h1><p>Content not found.</p></html>");
  }
  return -1;
}

int Stream::submit_file_response() {
  auto path = path_;
  if (path == "/") {
    path = "/index.html";
  }
  return submit_file_response(path);
}

void Stream::parse_path() {
  std::string_view reqpath = headers.path() ? headers.path().value() : "/";
  if (reqpath.empty()) {
    reqpath = "/";
  }

  auto query_pos = reqpath.find('?');
  std::string_view raw_path, raw_query;
  if (query_pos != reqpath.npos) {
    raw_path = reqpath.substr(0, query_pos);
    raw_query = reqpath.substr(query_pos);
  } else {
    raw_path = reqpath;
  }

  path_ = (raw_path.find('%') == raw_path.npos)
              ? raw_path
              : util::percent_decode(raw_path, mem_block_);
  query_ = (raw_query.find('%') == raw_query.npos)
               ? raw_query
               : util::percent_decode(raw_query, mem_block_);
}

int Stream::prepare_response() {

  // TODO: Implement push promise

  // TODO: Use Accept-Encoding to determine if compression is preferred

  // TODO: Don't do handler lookup for obvious file request and vice versa

  if (session_->get_server()->router_.dispatch_route(headers.method, path_,
                                                     &request_, &response_)) {
    return 0;
  }
  // unhandled, might be a file request
  if (headers.method == HttpMethod::GET) {
    return submit_file_response();
  } else {
    response_headers.status = "400";
    submit_json_response("<html><h1>400</h1><p>Bad Request.</p></html>");
  }
  return 0;
}
} // namespace hm
