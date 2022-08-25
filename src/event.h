#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <variant>

#include "dbnotify.h"
#include "dbresult.h"

namespace hm {

struct Event {
  std::string_view name;
  std::string_view channel;

  std::variant<std::string, std::string_view, db::SharedResultString,
               db::SharedNotify>
      content;

  Event(Event &&b)
      : name(b.name), channel(b.channel), content(std::move(b.content)) {}

  Event(const Event &b)
      : name(b.name), channel(b.channel), content(b.content) {}

  size_t length() const {
    return std::visit([](auto &&v) -> size_t { return v.length(); }, content);
  }
  std::string_view view() const {
    return std::visit([](auto &&v) -> std::string_view { return v; }, content);
  }

  Event(std::string_view channel, const std::string &payload) = delete;

  Event(std::string_view channel, const char *payload)
      : channel(channel), content(std::string_view(payload)) {
    name = channel.substr(0, channel.find('/'));
  }

  Event(std::string_view channel, std::string &&payload)
      : channel(channel), content(std::move(payload)) {
    name = channel.substr(0, channel.find('/'));
  }

  Event(std::string_view channel, std::string_view payload)
      : channel(channel), content(payload) {
    name = channel.substr(0, channel.find('/'));
  }

  Event(std::string_view channel, const db::SharedResultString &payload)
      : channel(channel), content(payload) {
    name = channel.substr(0, channel.find('/'));
  }

  Event(std::string_view channel, const db::SharedNotify &notif)
      : channel(channel), content(notif) {
    name = channel.substr(0, channel.find('/'));
  }
};
}; // namespace hm
