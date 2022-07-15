#pragma once

#include <deque>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <ev.h>
#include <nghttp2/nghttp2.h>

#include "server.h"

#include "filestream.h"

#include <readerwriterqueue.h>

class HttpSession;

class Worker {
  friend class HttpSession;

public:
  Worker(Server *server);
  ~Worker();

  void run();

  void add_connection(int fd);

  struct ssl_ctx_st *get_ssl_context();

  void accept_connection(int fd);

  void remove_session(HttpSession *session);

private:
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

  nghttp2_session_callbacks *callbacks_;
  nghttp2_option *options_;
};
