#include "eventstream.h"
#include "httpsession.h"
#include "stream.h"

namespace hm {

EventStream::EventStream(Stream *stream) : stream_(stream), paused_(false) {}

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
  auto len = ev.length();
  assert(pos_ + wlen <= ev.length());

  size_t nl = ev.name.length();

  size_t to_write = wlen;

  size_t ext_pre = l1 + nl + l2 + 1;

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
  }

  if (pos_ >= len) {
    // wrote this event fully
    queue_.pop_front();
  }

  return 0;
}

void EventStream::submit(Event &&event) {
  queue_.emplace_back(std::move(event));
  if (paused_) {
    nghttp2_session_resume_data(stream_->get_session()->get_nghttp2_session(),
                                stream_->id());
    paused_ = false;
  }
}

} // namespace hm
