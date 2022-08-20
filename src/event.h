#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "dbnotify.h"
#include "dbresult.h"

namespace hm {

struct Event {
  std::string_view name;
  std::string_view channel;

  std::variant<std::string, std::string_view, db::ResultString, db::Notify>
      content;

  size_t length() {
    return std::visit([](auto &&v) -> size_t { return v.length(); }, content);
  }
  std::string_view view() {
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

  Event(std::string_view channel, db::ResultString &&payload)
      : channel(channel), content(std::move(payload)) {
    name = channel.substr(0, channel.find('/'));
  }

  Event(std::string_view channel, db::Notify &&notif)
      : channel(channel), content(std::move(notif)) {
    name = channel.substr(0, channel.find('/'));
  }
};
}; // namespace hm
