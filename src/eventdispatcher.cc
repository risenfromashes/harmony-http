#include "eventdispatcher.h"
#include "worker.h"

#include <cassert>

namespace hm {

void EventDispatcher::suscribe(std::string &&channel_,
                               EventStream *event_stream) {
  std::string channel = std::move(channel_);

  if (auto itr = registry_.find(channel); itr != registry_.end()) {
    // check if any event stream in the vector was removed before
    auto &vec = itr->second;
    bool inserted = false;
    for (size_t i = 0; i < vec.size(); i++) {
      if (vec[i] == nullptr) {
        vec[i] = event_stream;
        inserted = true;
        break;
      }
    }
    if (!inserted) {
      itr->second.push_back(event_stream);
    }
  } else {
    registry_.try_emplace(std::move(channel),
                          std::vector<EventStream *>{event_stream});
  }
}

void EventDispatcher::remove(EventStream *event_stream) {
  for (auto &[_, vec] : registry_) {
    for (size_t i = 0; i < vec.size(); i++) {
      if (vec[i] == event_stream) {
        vec[i] = nullptr;
      }
    }
  }
}

void EventDispatcher::publish(Event &&event_) {
  Event event = std::move(event_);
  if (auto itr = registry_.find(event.channel); itr != registry_.end()) {
    for (auto es : itr->second) {
      if (es) {
        es->submit(std::move(event));
      }
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

void EventDispatcher::remove_stream(EventStream *es) { get()->remove(es); }

} // namespace hm
