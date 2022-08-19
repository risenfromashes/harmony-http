#include "stringstream.h"
#include "stream.h"
#include <nghttp2/nghttp2.h>

#include <iostream>

namespace hm {

StringStream::StringStream(std::string &&str) {
  data_ = std::move(str);
  auto &data = std::get<std::string>(data_);
  beg_ = last_ = data.data();
  end_ = beg_ + data.size();
}

StringStream::StringStream(std::string_view str) {
  beg_ = last_ = str.data();
  end_ = beg_ + str.size();
}

StringStream::StringStream(db::ResultString &&res) {
  data_ = std::move(res);
  std::string_view data = std::get<db::ResultString>(data_);
  beg_ = last_ = data.data();
  end_ = beg_ + data.size();
}

int StringStream::send(Stream *stream, size_t length) {
  auto wb = stream->get_buffer();

  assert(last_ + length <= end_);
  wb->write_full(last_, length);
  last_ += length;

  return 0;
}

} // namespace hm
