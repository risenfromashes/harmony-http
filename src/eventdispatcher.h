#pragma once

#include <string_view>
#include <unordered_map>

#include "eventstream.h"

namespace hm {

class EventDispatcher {

public:
  void suscribe(std::string &&channel, EventStream *event_stream);
  void publish(Event &&event);

  static EventDispatcher *get();
  static void listen(std::string &&channel, EventStream *event_stream);
  static void dispatch(Event &&event);

private:
  std::unordered_map<std::string_view, std::vector<EventStream *>> registry_;
};

} // namespace hm
