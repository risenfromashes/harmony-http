#include "eventstream.h"
#include "httpsession.h"
#include "stream.h"
#include "worker.h"

namespace hm {

EventStream::EventStream(Stream *stream) : stream_(stream), paused_(false) {
  pos_ = 0;
  ev_periodic_init(&evp_, EventStream::periodic_cb, 0, 2., 0);
  ev_periodic_start(Worker::get_worker()->get_loop(), &evp_);
  evp_.data = this;
}

EventStream::~EventStream() {
  ev_periodic_stop(Worker::get_worker()->get_loop(), &evp_);
}

int EventStream::send(Stream *stream, size_t wlen) {

  static const std::string_view s1 = "event: ";
  static const std::string_view s2 = "data: ";
  static const size_t l1 = s1.length();
  static const size_t l2 = s2.length();

  auto wb = stream->get_buffer();
  if (queue_.empty()) {
    return 0;
  }

  auto &ev = queue_.front();

  size_t nl = ev.name.length();
  size_t to_write = wlen;
  size_t ext_pre = l1 + nl + l2 + 1;
  size_t len = ext_pre + ev.length() + 2;

  assert(pos_ + wlen <= len);

  const char *start = (const char *)wb->last();

  if (pos_ == 0 && to_write >= ext_pre) {
    // faster path?
    wb->write_full(s1);
    wb->write_full(ev.name);
    wb->write_full("\n");
    wb->write_full(s2);
    pos_ += ext_pre;
    to_write -= ext_pre;
  } else {
    // conditionally check upto what we wrote
    while (to_write && pos_ < ext_pre) {
      std::string_view s;
      if (pos_ < l1) {
        // write "event: "
        s = s1.substr(pos_, to_write);
      } else if (pos_ < (l1 + nl)) {
        // write name
        s = ev.name.substr(pos_ - l1, to_write);
      } else if (pos_ == (l1 + nl)) {
        // write \n
        s = "\n";
      } else if (pos_ < (l1 + nl + 1 + l2)) {
        // write "data: "
        s = s2.substr(pos_ - (l1 + nl + 1), to_write);
      }
      wb->write_full(s);
      pos_ += s.length();
      to_write -= s.length();
    }
  }

  auto s = ev.view().substr(pos_ - ext_pre, to_write);
  wb->write_full(s);
  pos_ += s.length();
  to_write -= s.length();

  while (to_write--) {
    wb->write_byte('\n');
    pos_ += 1;
  }

  const char *last = (const char *)wb->last();

  if (pos_ >= len) {
    // wrote this event fully
    queue_.pop_front();
    // reset position
    pos_ = 0;
  }

  return 0;
}

void EventStream::submit(Event &&event) {
  queue_.emplace_back(std::move(event));
  if (paused_) {
    auto rt = nghttp2_session_resume_data(
        stream_->get_session()->get_nghttp2_session(), stream_->id());

    // reset offset
    pos_ = 0;
    paused_ = false;
    stream_->get_session()->start_write();
  }
}

void EventStream::ping() { submit(Event("ping", "Hello!")); }

void EventStream::periodic_cb(struct ev_loop *loop, ev_periodic *w,
                              int revents) {
  auto self = static_cast<EventStream *>(w->data);
  self->ping();
}

size_t EventStream::length() {
  if (queue_.empty()) {
    paused_ = true;
    return 0;
  }

  // event: <name>\n
  // data: <data>\n
  //\n
  // (three new lines)
  static const size_t extra =
      std::strlen("event: ") + std::strlen("data: ") + 3;

  auto &ev = queue_.front();
  auto rt = ev.name.length() + ev.length() + extra;

  return rt;
}
} // namespace hm
