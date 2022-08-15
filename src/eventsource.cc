#include "eventsource.h"

#include "eventdispatcher.h"
#include "eventstream.h"
#include "stream.h"

namespace hm {

EventSource::EventSource(HttpRequest *req, HttpResponse *res) {
  res->set_header("cache-control", "no-store");
  res->set_header("content-type", "text/event-stream");
  event_stream_ = res->stream_->add_data_stream<EventStream>(res->stream_);
  res->stream_->submit_response(event_stream_);
}

void EventSource::suscribe(std::string &&channel) {
  EventDispatcher::listen(std::move(channel), event_stream_);
}

void EventSource::send(Event &&event) {
  event_stream_->submit(std::move(event));
}

}; // namespace hm
