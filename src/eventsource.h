#pragma once

#include "event.h"
#include "httprequest.h"
#include "httpresponse.h"

namespace hm {

class EventStream;

class EventSource {
public:
  EventSource(HttpRequest *req, HttpResponse *res);

  void suscribe(std::string &&channel);

  void send(Event &&event);

private:
  EventStream *event_stream_;
};

} // namespace hm
