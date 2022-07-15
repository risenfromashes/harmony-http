#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "datastream.h"

class FileStream : public DataStream {

  friend class Server;
  struct FileInfo;

  FileStream(int fd, std::string path, struct ev_loop *loop);

public:
  static std::unique_ptr<FileStream> create(std::string path,
                                            struct ev_loop *loop);
  FileStream(const FileStream &) = delete;
  FileStream &operator=(const FileStream &) = delete;

  ~FileStream();

  static int on_send(DataStream *self, Stream *stream, size_t length);

  size_t length();
  std::string_view path() { return path_; }
  std::string_view mime_type() { return mime_type_; }

  const FileInfo &get_info();

private:
  void set_mime_type(const std::string &path);
  void update_info(const FileInfo &info);

  static void update_cb(struct ev_loop *loop, struct ev_stat *w, int revents);

private:
  int fd_;
  std::string path_;
  std::string_view mime_type_;

  std::atomic<bool> updated_;

  struct ev_loop *loop_;
  struct ev_stat *stat_watcher_;

  struct FileInfo {
    int64_t mtime;
    int64_t length;
  } info_, new_info_;
};
