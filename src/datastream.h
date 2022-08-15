#pragma once

#include <cstdint>
#include <cstdlib>
#include <memory>

namespace hm {
class Stream;

struct DataStream {
  DataStream() = default;
  DataStream(const DataStream &) = delete;
  DataStream &operator=(const DataStream &) = delete;

  virtual int send(Stream *stream, size_t length) = 0;
  virtual size_t length() = 0;
  virtual size_t offset() = 0;
  virtual std::pair<size_t, bool> remaining() = 0;
};
} // namespace hm
