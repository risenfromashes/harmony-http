#pragma once

#include <cstdint>
#include <deque>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <ev.h>
#include <nghttp2/nghttp2.h>

#include "dbsession.h"
#include "filestream.h"
#include "server.h"

#include <readerwriterqueue.h>

namespace hm {

class HttpSession;

class Worker {
  friend class Server;
  friend class HttpSession;
  friend class Stream;

public:
  Worker(Server *server);
  ~Worker();

  void run();

  void cancel();

  void add_connection(int fd);

  struct ssl_ctx_st *get_ssl_context();

  void accept_connection(int fd);

  void remove_session(HttpSession *session);

  void remove_static_file(FileStream *file);
  FileStream *get_static_file(const std::string_view &rel_path,
                              bool prefer_compressed = true);
  std::string_view get_static_root();

  std::string_view get_cached_date();

  Server *get_server();
  struct ev_loop *get_loop();

  void start_db_session(const char *connect_string);
  void restart_db_session();
  db::Session *get_db_session() { return dbsession_.get(); }

  bool is_stream_alive(uint64_t serial);

private:
  void add_stream(Stream *stream);
  void remove_stream(Stream *stream);

  FileStream *add_static_file(std::string path);

  static void async_acceptcb(struct ev_loop *loop, ev_async *watcher,
                             int revents);
  static void async_cancelcb(struct ev_loop *loop, ev_async *watcher,
                             int revents);

  static void periodic_cb(struct ev_loop *loop, ev_periodic *watcher,
                          int revents);

private:
  Server *server_;
  struct ev_loop *loop_;
  ev_periodic periodic_watcher_;
  ev_async async_watcher_;
  ev_async cancel_watcher_;
  std::unique_ptr<std::thread> th_;
  std::mutex mutex_;
  moodycamel::ReaderWriterQueue<int> queued_fds_;

  std::list<HttpSession> sessions_;

  const char *dbconnection_string_;
  std::unique_ptr<db::Session> dbsession_;

  nghttp2_session_callbacks *callbacks_;
  nghttp2_option *options_;

  std::multimap<std::string_view, std::unique_ptr<FileStream>> files_;

  struct DateCache {
    char mem[29];
    std::string_view date;
    time_t cache_time;
    DateCache() {
      date = std::string_view(mem, 29);
      cache_time = std::numeric_limits<time_t>::min();
    }
  } date_cache_;

  uint64_t next_stream_serial_ = 0;
  std::unordered_set<uint64_t> alive_streams_;

  bool started_ = false;
};
} // namespace hm
