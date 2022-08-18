#include "filestream.h"
#include "stream.h"

namespace hm {

FileStream::FileStream(FileEntry *file_entry)
    : file_(file_entry), length_(file_entry->info().length) {
  pos_ = 0;
}

int FileStream::send(Stream *stream, size_t wlen) {

  assert(0 <= wlen && wlen <= remaining().first);
  auto wb = stream->get_buffer();

  while (wlen) {
    ssize_t nread;
    while ((nread = pread(file_->fd(), wb->last(), wlen, pos_)) == -1 &&
           errno == EINTR)
      ;

    if (nread == -1) {
      std::cerr << "Read from file: " << file_->path() << " failed."
                << std::endl;
      std::cerr << "Error: " << strerror(errno) << std::endl;

      stream->stop_read_timeout();
      stream->stop_write_timeout();
      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    wlen -= nread;
    wb->write(nread);

    pos_ += nread;
  }

  return 0;
}

} // namespace hm
