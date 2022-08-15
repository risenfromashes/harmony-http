#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "datastream.h"

namespace hm {

class Worker;

class FileEntry {

  friend class Server;
  struct FileInfo;

  FileEntry(int fd, std::string path, Worker *worker);

public:
  static std::unique_ptr<FileEntry> create(std::string path, Worker *worker);
  FileEntry(const FileEntry &) = delete;
  FileEntry &operator=(const FileEntry &) = delete;

  ~FileEntry();

  int fd() { return fd_; }

  std::string_view path() { return path_; }
  // relative path ignoring encoding suffix
  std::string_view relpath() { return relpath_; }

  std::string_view mime_type() { return mime_type_; }

  const FileInfo &info();

  std::string_view ext() { return ext_; }

  bool compressed() { return compressed_; }

  std::string_view encoding() { return encoding_; }

  void remove_self();

private:
  void check_if_compressed(const std::string_view &path);
  void set_ext(const std::string_view &path);
  void set_mime_type(const std::string_view &path);
  void update_info(const FileInfo &info);

  static void update_cb(struct ev_loop *loop, struct ev_stat *w, int revents);

private:
  int fd_;
  std::string path_;
  std::string relpath_;
  std::string ext_;
  std::string mime_type_;
  bool compressed_;
  const char *encoding_;

  std::atomic<bool> updated_;

  Worker *worker_;
  struct ev_stat *stat_watcher_;

  struct FileInfo {
    int64_t mtime;
    int64_t length;
  } info_, new_info_;
};
} // namespace hm
