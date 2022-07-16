#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "datastream.h"

namespace hm {

class Worker;

class FileStream : public DataStream {

  friend class Server;
  struct FileInfo;

  FileStream(int fd, std::string path, Worker *worker);

public:
  static std::unique_ptr<FileStream> create(std::string path, Worker *worker);
  FileStream(const FileStream &) = delete;
  FileStream &operator=(const FileStream &) = delete;

  ~FileStream();

  static int on_send(DataStream *self, Stream *stream, size_t length);

  size_t length();

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
