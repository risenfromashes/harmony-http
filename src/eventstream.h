#pragma once

#include "datastream.h"

#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>
#include <variant>

#include <ev.h>

#include "dbresult.h"
#include "event.h"

#include <iostream>

namespace hm {

class EventStream : public DataStream {
public:
public:
  EventStream(Stream *stream);
  ~EventStream();

  int send(Stream *stream, size_t length) override;

  size_t length() override;

  size_t offset() override { return pos_; }

  std::pair<size_t, bool> remaining() override {
    return {length() - pos_, false};
  }

  void submit(Event &&event);

  void ping();

private:
  static void periodic_cb(struct ev_loop *loop, ev_periodic *w, int revents);

private:
  Stream *stream_;
  size_t pos_;

  ev_periodic evp_;

  std::deque<Event> queue_;
  bool paused_;
};
} // namespace hm
