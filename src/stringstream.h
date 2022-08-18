#pragma once

#include "datastream.h"

#include <cstdint>
#include <memory>

namespace hm {

class StringStream : public DataStream {

public:
  StringStream(std::string_view str);
  StringStream(std::string &&str);
  StringStream(const std::string &) = delete;

  int send(Stream *stream, size_t length) override;

  size_t length() override { return end_ - beg_; }
  size_t offset() override { return last_ - beg_; }
  std::pair<size_t, bool> remaining() override { return {end_ - last_, true}; }

private:
  std::string data_;
  const char *beg_, *last_, *end_;
};
} // namespace hm
