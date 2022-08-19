#include "eventdispatcher.h"
#include "worker.h"

#include <cassert>

namespace hm {

void EventDispatcher::suscribe(std::string &&channel_,
                               EventStream *event_stream) {
  std::string channel = std::move(channel_);
  if (auto itr = registry_.find(channel); itr != registry_.end()) {
    itr->second.push_back(event_stream);
  } else {
    {
      auto [itr, inserted] = registry_.try_emplace(std::move(channel));
      assert(inserted);
      itr->second.push_back(event_stream);
    }
  }
}

void EventDispatcher::publish(Event &&event_) {
  Event event = std::move(event_);
  if (auto itr = registry_.find(event.channel); itr != registry_.end()) {
    for (auto es : itr->second) {
      es->submit(std::move(event));
    }
  }
}

EventDispatcher *EventDispatcher::get() {
  return Worker::get_worker()->get_event_dispatcher();
}

void EventDispatcher::listen(std::string &&channel, EventStream *event_stream) {
  get()->suscribe(std::move(channel), event_stream);
}

void EventDispatcher::dispatch(Event &&event) {
  get()->publish(std::move(event));
}

} // namespace hm
