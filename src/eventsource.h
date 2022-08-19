#pragma once

#include "event.h"
#include "httprequest.h"
#include "httpresponse.h"

namespace hm {

class EventStream;

class EventSource {
  EventSource(EventStream *stream);

public:
  static EventSource create(HttpRequest *req, HttpResponse *res);

  void suscribe(std::string &&channel);
  void suscribe(std::string_view channel);

  void send(Event &&event);

private:
  EventStream *event_stream_;
};

} // namespace hm
