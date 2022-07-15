#pragma once

#include "datastream.h"

#include <cstdint>
#include <memory>

class StringStream : public DataStream {

public:
  explicit StringStream(std::string_view str);
  explicit StringStream(std::string &&str);

  static int on_send(DataStream *self, Stream *stream, size_t length);

  size_t length() override;

private:
  std::string data_;
  const char *beg_, *last_, *end_;
};
