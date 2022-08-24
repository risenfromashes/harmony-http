#pragma once

#include <string_view>
#include <unordered_map>

#include "eventstream.h"

#include "util.h"

namespace hm {

class EventDispatcher {

public:
  void suscribe(std::string &&channel, EventStream *event_stream);
  void publish(Event &&event);
  void remove(EventStream *stream);

  static EventDispatcher *get();
  static void listen(std::string &&channel, EventStream *event_stream);
  static void dispatch(Event &&event);

  static void remove_stream(EventStream *);

private:
  util::string_map<std::vector<EventStream *>> registry_;
};

} // namespace hm
