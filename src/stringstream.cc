#include "stringstream.h"
#include "stream.h"
#include <nghttp2/nghttp2.h>

#include <iostream>

namespace hm {

StringStream::StringStream(std::string &&str) : DataStream(on_send) {
  data_ = std::move(str);
  beg_ = last_ = data_.data();
  end_ = beg_ + data_.size();
}
StringStream::StringStream(std::string_view str) : DataStream(on_send) {
  beg_ = last_ = str.data();
  end_ = beg_ + str.size();
}

int StringStream::on_send(DataStream *ds, Stream *stream, size_t length) {
  auto self = static_cast<StringStream *>(ds);
  auto wb = stream->get_buffer();

  assert(self->last_ + length <= self->end_);
  wb->write_full(self->last_, length);
  self->last_ += length;

  return 0;
}

size_t StringStream::length() { return end_ - beg_; }
} // namespace hm
