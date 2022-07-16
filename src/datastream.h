#pragma once

#include <cstdint>
#include <cstdlib>

namespace hm {
class Stream;

struct DataStream {

  int (*send_data)(DataStream *self, Stream *stream, size_t length);

  DataStream(const DataStream &) = delete;
  DataStream &operator=(const DataStream &) = delete;

  virtual size_t length() = 0;

protected:
  DataStream(decltype(send_data) send_data_cb) : send_data(send_data_cb) {}
};
} // namespace hm
