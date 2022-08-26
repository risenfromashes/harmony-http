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

FileEntry::FileEntry(int fd, std::string p, Worker *worker, bool watch)
    : fd_(fd), path_(std::move(p)), worker_(worker), compressed_(false),
      watch_(watch), encoding_("\0") {

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

  if (watch_) {
    stat_watcher_ = static_cast<ev_stat *>(malloc(sizeof(ev_stat)));
    ev_stat_init(stat_watcher_, update_cb, path_.c_str(), 0.0);
    stat_watcher_->data = this;
    ev_stat_start(worker_->get_loop(), stat_watcher_);
  }
}

std::unique_ptr<FileEntry> FileEntry::create(std::string path, Worker *worker,
                                             bool watch) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    std::cerr << "Failed to open file: " << path << std::endl;
    return nullptr;
  }
  return std::unique_ptr<FileEntry>(
      new FileEntry(fd, std::move(path), worker, watch));
}

FileEntry::~FileEntry() {
  // std::cout << "File removed: " << path_ << std::endl;
  if (watch_) {
    ev_stat_stop(worker_->get_loop(), stat_watcher_);
    free(stat_watcher_);
    close(fd_);
  }
}

void FileEntry::update_cb(struct ev_loop *loop, ev_stat *w, int revents) {
  // std::cerr << "File updated: " << w->path << std::endl;
  auto self = static_cast<FileEntry *>(w->data);
  if (w->attr.st_nlink) {
    self->update_info(
        FileInfo{.mtime = w->attr.st_mtime, .length = w->attr.st_size});
  } else {
    self->remove_self();
  }
}

const FileEntry::FileInfo &FileEntry::info() {
  bool tr = true;
  if (updated_.compare_exchange_strong(tr, false)) {
    // updated
    // std::cout << "File info updated: " << path_ << std::endl;
    info_ = new_info_;
  }
  return info_;
}

void FileEntry::update_info(const FileEntry::FileInfo &info) {
  new_info_ = info;
  updated_.store(true);
}

void FileEntry::check_if_compressed(const std::string_view &path) {
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

void FileEntry::set_ext(const std::string_view &path) {
  ext_ = path.substr(path.find_last_of('.') + 1);
}

void FileEntry::set_mime_type(const std::string_view &path) {
  static auto dict = util::read_mime_types().first;

  auto itr = dict.find(ext_);
  if (itr != dict.end()) {
    mime_type_ = itr->second;
  }
}

void FileEntry::remove_self() { worker_->remove_static_file(this); }

} // namespace hm
