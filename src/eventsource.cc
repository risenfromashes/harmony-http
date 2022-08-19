#include "eventsource.h"

#include "eventdispatcher.h"
#include "eventstream.h"
#include "httpsession.h"
#include "stream.h"

#include <stacktrace>

namespace hm {

EventSource::EventSource(EventStream *stream) : event_stream_(stream) {}

EventSource EventSource::create(HttpRequest *req, HttpResponse *res) {
  res->set_header("cache-control", "no-store");
  res->set_header("content-type", "text/event-stream");

  auto evstream = res->stream_->add_data_stream<EventStream>(res->stream_);

  res->stream_->stop_read_timeout();
  // stopping read timeout has no effect here
  // since its reset after sending frame

  res->stream_->submit_response(evstream);
  return EventSource(evstream);
}

void EventSource::suscribe(std::string &&channel) {
  EventDispatcher::listen(std::move(channel), event_stream_);
}

void EventSource::suscribe(std::string_view channel) {
  EventDispatcher::listen(std::string(channel), event_stream_);
}

void EventSource::send(Event &&event) {
  event_stream_->submit(std::move(event));
}

}; // namespace hm
