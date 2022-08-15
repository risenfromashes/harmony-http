/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <string_view>

namespace hm {
// a thin wrapper around std::array based buffer
template <size_t N> struct Buffer {

  Buffer() : pos_(std::begin(buf_)), last_(pos_) {}

  // Resets the buffer by making both pos and last point at the beginning
  void reset() { pos_ = last_ = std::begin(buf_); }

  // Returns the number of bytes left to read
  size_t rleft() const { return last_ - pos_; }

  // Returns the number of bytes this buffer can store.
  size_t wleft() const { return std::end(buf_) - last_; }

  // Writes upto min(wleft(), count) bytes from buffer pointed to by src.
  // Returns the number of bytes written.
  size_t write(const void *src, size_t count) {
    count = std::min(count, wleft());
    auto *p = static_cast<const uint8_t *>(src);
    last_ = std::copy_n(p, count, last_);
    return count;
  }

  // makes sures there is space for |count| bytes and writes them
  void write_full(const void *src, size_t count) {
    assert(count <= wleft());
    auto *p = static_cast<const uint8_t *>(src);
    last_ = std::copy_n(p, count, last_);
  }

  // makes sures there is space for |count| bytes and writes them
  void write_full(std::string_view src) {
    write_full(src.data(), src.length());
  }

  void write_byte(uint8_t byte) { *last_++ = byte; }

  void fill(uint8_t v, size_t count) {
    assert(count <= wleft());
    std::fill(last_, last_ + count, v);
    last_ += count;
  }

  // Updates buffer state as if write(..., count) was called
  size_t write(size_t count) {
    count = std::min(count, wleft());
    last_ += count;
    return count;
  }

  // Drains min(rleft(), count) bytes from the start of the buffer.
  // Returns the number of bytes drained.
  size_t drain(size_t count) {
    count = std::min(count, rleft());
    pos_ += count;
    return count;
  }

  // Drains min(rleft(), count) bytes from the start of the buffer and
  // copies remaining data to the beginning of the buffer, freeing up space.
  // Returns the number of bytes drained.
  size_t drain_reset(size_t count) {
    count = std::min(count, rleft());
    auto beg = std::begin(buf_);
    std::copy(pos_ + count, last_, beg);
    last_ = beg + (last_ - (pos_ + count));
    pos_ = beg;
    return count;
  }

  uint8_t *begin() { return std::begin(buf_); }
  uint8_t *end() { return std::end(buf_); }

  uint8_t &operator[](size_t n) {
    assert(n < N);
    return buf_[n];
  }

  const uint8_t &operator[](size_t n) const {
    assert(n < N);
    return buf_[n];
  }

  const uint8_t *pos() const { return pos_; }
  const uint8_t *last() const { return last_; }

  uint8_t *pos() { return pos_; }
  uint8_t *last() { return last_; }

private:
  std::array<uint8_t, N> buf_;
  uint8_t *pos_, *last_;
};
} // namespace hm
