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
#include "worker.h"

namespace hm {

FileStream::FileStream(int fd, std::string p, Worker *worker)
    : DataStream(on_send), fd_(fd), path_(std::move(p)), worker_(worker),
      compressed_(false), encoding_("\0") {

  relpath_ = path_.substr(worker_->get_static_root().size() - 1);
  assert(fd >= 0);
  set_ext(path_);
  check_if_compressed(path_);
  set_mime_type(path_);
  // if (compressed()) {
  //   std::cout << "file: " << path() << std::endl
  //             << "ext: " << ext() << std::endl
  //             << "encoding: " << encoding() << std::endl
  //             << "mime_type: " << mime_type() << std::endl;
  // }

  struct stat st;
  fstat(fd_, &st);

  info_ = {.mtime = st.st_mtime, .length = st.st_size};

  stat_watcher_ = static_cast<ev_stat *>(malloc(sizeof(ev_stat)));
  ev_stat_init(stat_watcher_, update_cb, path_.c_str(), 0.0);
  stat_watcher_->data = this;
  ev_stat_start(worker_->get_loop(), stat_watcher_);
}

std::unique_ptr<FileStream> FileStream::create(std::string path,
                                               Worker *worker) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    std::cerr << "Failed to open file: " << path << std::endl;
    return nullptr;
  }
  return std::unique_ptr<FileStream>(
      new FileStream(fd, std::move(path), worker));
}

FileStream::~FileStream() {
  // std::cout << "File removed: " << path_ << std::endl;
  ev_stat_stop(worker_->get_loop(), stat_watcher_);
  free(stat_watcher_);
  close(fd_);
}

void FileStream::update_cb(struct ev_loop *loop, ev_stat *w, int revents) {
  // std::cerr << "File updated: " << w->path << std::endl;
  auto self = static_cast<FileStream *>(w->data);
  if (w->attr.st_nlink) {
    self->update_info(
        FileInfo{.mtime = w->attr.st_mtime, .length = w->attr.st_size});
  } else {
    self->remove_self();
  }
}

size_t FileStream::length() { return info().length; }

const FileStream::FileInfo &FileStream::info() {
  bool tr = true;
  if (updated_.compare_exchange_strong(tr, false)) {
    // updated
    // std::cout << "File info updated: " << path_ << std::endl;
    info_ = new_info_;
  }
  return info_;
}

void FileStream::update_info(const FileStream::FileInfo &info) {
  new_info_ = info;
  updated_.store(true);
}

void FileStream::check_if_compressed(const std::string_view &path) {
  if (ext_ == "br") {
    compressed_ = true;
    encoding_ = "br";
    set_ext(path.substr(0, path.find_last_of('.')));
    relpath_ = relpath_.substr(0, relpath_.find_last_of('.'));
  } else if (ext_ == "gzip" || ext_ == "gz") {
    compressed_ = true;
    encoding_ = "gzip";
    set_ext(path.substr(0, path.find_last_of('.')));
    relpath_ = relpath_.substr(0, relpath_.find_last_of('.'));
  }
}

void FileStream::set_ext(const std::string_view &path) {
  ext_ = path.substr(path.find_last_of('.') + 1);
}

void FileStream::set_mime_type(const std::string_view &path) {
  static auto dict = util::read_mime_types();

  auto itr = dict.find(ext_);
  if (itr != dict.end()) {
    mime_type_ = itr->second;
  }
}

void FileStream::remove_self() { worker_->remove_static_file(this); }

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
} // namespace hm
