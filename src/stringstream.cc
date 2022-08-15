#include "stringstream.h"
#include "stream.h"
#include <nghttp2/nghttp2.h>

#include <iostream>

namespace hm {

StringStream::StringStream(std::string &&str) {
  data_ = std::move(str);
  beg_ = last_ = data_.data();
  end_ = beg_ + data_.size();
}

StringStream::StringStream(std::string_view str) {
  beg_ = last_ = str.data();
  end_ = beg_ + str.size();
}

int StringStream::send(Stream *stream, size_t length) {
  auto wb = stream->get_buffer();

  assert(last_ + length <= self->end_);
  wb->write_full(last_, length);
  last_ += length;

  return 0;
}

} // namespace hm
