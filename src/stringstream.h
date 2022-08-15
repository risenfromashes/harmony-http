#pragma once

#include "datastream.h"

#include <cstdint>
#include <memory>

namespace hm {

class StringStream : public DataStream {

public:
  explicit StringStream(std::string_view str);
  explicit StringStream(std::string &&str);
  StringStream(const std::string &) = delete;
  StringStream(std::string) = delete;

  int send(Stream *stream, size_t length) override;

  size_t length() override { return end_ - beg_; }
  size_t offset() override { return last_ - beg_; }
  std::pair<size_t, bool> remaining() override { return {end_ - last_, false}; }

private:
  std::string data_;
  const char *beg_, *last_, *end_;
};
} // namespace hm
