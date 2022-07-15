#include <fcntl.h>

#include <cassert>
#include <iostream>
#include <string_view>

#include <ev.h>
#include <nghttp2/nghttp2.h>
#include <sys/stat.h>

#include "filestream.h"
#include "stream.h"
#include "util.h"

FileStream::FileStream(int fd, std::string p, struct ev_loop *loop)
    : DataStream(on_send), fd_(fd), path_(std::move(p)), loop_(loop) {
  assert(fd >= 0);
  set_mime_type(path_);

  struct stat st;
  fstat(fd_, &st);

  info_ = {.mtime = st.st_mtime, .length = st.st_size};

  stat_watcher_ = static_cast<ev_stat *>(malloc(sizeof(ev_stat)));
  ev_stat_init(stat_watcher_, update_cb, path_.c_str(), 0.0);
  stat_watcher_->data = this;
  ev_stat_start(loop_, stat_watcher_);
}

std::unique_ptr<FileStream> FileStream::create(std::string path,
                                               struct ev_loop *loop) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    std::cerr << "Failed to open file: " << path << std::endl;
    return nullptr;
  }
  return std::unique_ptr<FileStream>(new FileStream(fd, std::move(path), loop));
}

FileStream::~FileStream() {
  ev_stat_stop(loop_, stat_watcher_);
  free(stat_watcher_);
  close(fd_);
}

void FileStream::update_cb(struct ev_loop *loop, ev_stat *w, int revents) {
  auto self = static_cast<FileStream *>(w->data);
  if (w->attr.st_nlink) {
    self->update_info(
        FileInfo{.mtime = w->attr.st_mtime, .length = w->attr.st_size});
  }
}

size_t FileStream::length() { return get_info().length; }

const FileStream::FileInfo &FileStream::get_info() {
  bool tr = true;
  if (updated_.compare_exchange_strong(tr, false)) {
    // updated
    info_ = new_info_;
  }
  return info_;
}

void FileStream::update_info(const FileStream::FileInfo &info) {
  new_info_ = info;
  updated_.store(true);
}

void FileStream::set_mime_type(const std::string &path) {
  static auto dict = util::read_mime_types();

  const char *ext = path.c_str() + (path.find_last_of('.') + 1);
  auto itr = dict.find(ext);
  if (itr != dict.end()) {
    mime_type_ = itr->second;
  }
}

int FileStream::on_send(DataStream *ds, Stream *stream, size_t length) {

  const auto *self = static_cast<FileStream *>(ds);
  auto wb = stream->get_buffer();

  while (length) {
    ssize_t nread;
    while ((nread = pread(self->fd_, wb->last(), length,
                          stream->body_offset_)) == -1 &&
           errno == EINTR)
      ;

    if (nread == -1) {
      std::cerr << "Read from file: " << self->path_ << " failed." << std::endl;
      std::cerr << "Error: " << strerror(errno) << std::endl;

      stream->stop_read_timeout();
      stream->stop_write_timeout();
      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }

    length -= nread;
    wb->write(nread);
  }

  return 0;
}
