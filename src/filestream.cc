#include "filestream.h"
#include "stream.h"

namespace hm {

int FileStream::send(Stream *stream, size_t length) {

  assert(length < left());
  auto wb = stream->get_buffer();

  while (length) {
    ssize_t nread;
    while ((nread = pread(file_->fd(), wb->last(), length, pos_)) == -1 &&
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

    length_ -= nread;
    pos_ += nread;
    wb->write(nread);
  }

  return 0;
}

} // namespace hm
