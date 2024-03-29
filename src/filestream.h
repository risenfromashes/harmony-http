#pragma once

#include "datastream.h"
#include "fileentry.h"

#include <cstdint>

namespace hm {

class FileStream : public DataStream {

public:
  FileStream(FileEntry *file_entry);

  int send(Stream *stream, size_t length) override;

  size_t length() override { return length_; }
  size_t offset() override { return pos_; }
  std::pair<size_t, bool> remaining() override {
    return {length_ - pos_, true};
  }

private:
  size_t length_;
  size_t pos_;
  FileEntry *file_;
};
} // namespace hm
