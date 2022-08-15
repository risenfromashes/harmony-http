#pragma once

#include "datastream.h"

#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>
#include <variant>

#include "dbresult.h"
#include "event.h"

namespace hm {

class EventStream : public DataStream {
public:
public:
  EventStream(Stream *stream);

  int send(Stream *stream, size_t length) override;

  size_t length() override {
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
    return ev.name.length() + ev.length() + extra;
  }

  size_t offset() override { return pos_; }

  std::pair<size_t, bool> remaining() override {
    return {length() - pos_, false};
  }

  void submit(Event &&event);

private:
  Stream *stream_;
  size_t pos_;

  std::deque<Event> queue_;
  bool paused_;
};
} // namespace hm
