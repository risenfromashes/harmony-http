#include "worker.h"
#include "httpsession.h"
#include "server.h"
#include "util.h"

#include <iostream>

#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>

Worker::Worker(Server *server) : server_(server), queued_fds_(100) {
  // initialise new event loop
  loop_ = ev_loop_new(ev_recommended_backends());
  // initialise watcher to send new socket events from main thread
  ev_async_init(&async_watcher_, async_acceptcb);
  // initialise watcher to cancel event loop
  ev_async_init(&cancel_watcher_, async_cancelcb);
  // initialise periodic watcher to check health
  // ev_periodic_init(&periodic_watcher_, periodic_cb, 0., 30., nullptr);

  async_watcher_.data = this;
  cancel_watcher_.data = this;
  periodic_watcher_.data = this;
  ev_async_start(loop_, &async_watcher_);
  ev_async_start(loop_, &cancel_watcher_);
  // ev_periodic_start(loop_, &periodic_watcher_);

  nghttp2_session_callbacks_new(&callbacks_);
  HttpSession::fill_callback(callbacks_);

  nghttp2_option_new(&options_);
}

Worker::~Worker() {
  ev_async_send(loop_, &cancel_watcher_);
  if (th_->joinable()) {
    th_->join();
  }
  // need to destroy sessions first which use loop_
  sessions_.clear();
  nghttp2_session_callbacks_del(callbacks_);
  nghttp2_option_del(options_);
  ev_loop_destroy(loop_);
}

// callback called from main thread when new socket is accepted
void Worker::async_acceptcb(struct ev_loop *loop, ev_async *watcher,
                            int revents) {
  auto self = static_cast<Worker *>(watcher->data);

  int fd;
  while (self->queued_fds_.try_dequeue(fd)) {
    self->accept_connection(fd);
  }
}

void Worker::add_connection(int fd) {
  queued_fds_.emplace(fd);
  ev_async_send(loop_, &async_watcher_);
}

void Worker::async_cancelcb(struct ev_loop *loop, ev_async *watcher,
                            int revents) {
  auto self = static_cast<Worker *>(watcher->data);
  ev_break(self->loop_, EVBREAK_ALL);
}

void Worker::periodic_cb(struct ev_loop *loop, ev_periodic *watcher,
                         int revents) {
  // auto self = static_cast<Worker *>(watcher->data);
  // std::cerr << "thread " << std::this_thread::get_id() << " is alive"
  //           << std::endl;
}

SSL_CTX *Worker::get_ssl_context() { return server_->ssl_ctx_.get(); }

void Worker::run() {
  th_ = std::make_unique<std::thread>(
      [](struct ev_loop *loop) { ev_run(loop, 0); }, loop_);
}

void Worker::accept_connection(int fd) {
  util::make_socket_nodelay(fd);
  auto ssl = HttpSession::create_ssl_session(get_ssl_context(), fd);
  if (ssl) {
    auto &session = sessions_.emplace_front(this, loop_, fd, std::move(ssl));
    session.itr_ = sessions_.begin();
  } else {
    std::cerr << "Failed to create ssl session. Rejecting connection."
              << std::endl;
    close(fd);
  }
}

void Worker::remove_session(HttpSession *session) {
  sessions_.erase(session->itr_);
}
