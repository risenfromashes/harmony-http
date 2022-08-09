#include "worker.h"
#include "httpsession.h"
#include "server.h"
#include "util.h"

#include <iomanip>
#include <iostream>

#include <dirent.h>
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <unistd.h>

namespace hm {

Worker::Worker(Server *server)
    : server_(server), queued_fds_(100), dbsession_(nullptr) {
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
  if (started_) {
    ev_async_send(loop_, &cancel_watcher_);
    if (th_->joinable()) {
      th_->join();
    }
  }
  // need to destroy sessions first which use loop_
  sessions_.clear();
  if (dbsession_) {
    dbsession_.reset();
  }
  nghttp2_session_callbacks_del(callbacks_);
  nghttp2_option_del(options_);
  files_.clear();
  ev_loop_destroy(loop_);
}

void Worker::cancel() {
  if (started_) {
    ev_break(loop_, EVBREAK_ALL);
  }
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

struct ssl_ctx_st *Worker::get_ssl_context() {
  return server_->ssl_ctx_.get();
}

void Worker::run() {
  started_ = true;
  th_ = std::make_unique<std::thread>(
      [](struct ev_loop *loop) { ev_run(loop, 0); }, loop_);
  workers_.emplace(th_->get_id(), this);
}

void Worker::accept_connection(int fd) {
  util::make_socket_nodelay(fd);
  auto ssl = HttpSession::create_ssl_session(get_ssl_context(), fd);
  if (ssl) {
    auto &session = sessions_.emplace_front(this, fd, std::move(ssl));
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

void Worker::remove_static_file(FileStream *file) {
  files_.erase(file->relpath());
}

FileStream *Worker::add_static_file(std::string path) {
  if (access(path.c_str(), R_OK) != 0) {
    return nullptr;
  }
  auto fs = FileStream::create(std::move(path), this);
  if (fs) {
    auto *ptr = fs.get();
    // use relative path as key, including opening /
    auto relpath = fs->relpath();
    // std::cout << "Serving static file: " << std::left << std::setw(40)
    //           << relpath << " [" << std::left << std::setw(40) << fs->path()
    //           << "]" << std::endl;
    files_.emplace(relpath, std::move(fs));

    return ptr;
  }
  return nullptr;
}

FileStream *Worker::get_static_file(const std::string_view &path,
                                    bool prefer_compressed) {
  auto [beg, end] = files_.equal_range(path);
  FileStream *ret = nullptr;
  for (auto itr = beg; itr != end; ++itr) {
    ret = itr->second.get();
    if (prefer_compressed && ret->compressed()) {
      return ret;
    }
    if (!prefer_compressed && !ret->compressed()) {
      return ret;
    }
  }
  if (!ret) {
    // file doesn't exist yet
    // try to add it
    ret = add_static_file(server_->static_root_ + std::string(path));
    // also check if there is compressed files available

    if (prefer_compressed) {
      auto br =
          add_static_file(server_->static_root_ + std::string(path) + ".br");
      if (br) {
        return br;
      }
    }
  }
  return ret;
}

std::string_view Worker::get_static_root() { return server_->static_root_; }

std::string_view Worker::get_cached_date() {
  auto now = ev_now(loop_);
  if (now >= date_cache_.cache_time) {
    // update
    util::http_date(now, date_cache_.mem);
  }
  return date_cache_.date;
}

Server *Worker::get_server() { return server_; }

struct ev_loop *Worker::get_loop() {
  return loop_;
}

void Worker::start_db_session(const char *connect_string) {
  dbconnection_string_ = connect_string;
  dbsession_ = db::Session::create(this, connect_string, query_dir_);
  if (!dbsession_) {
    std::cerr << "Failed to start db session. Worker exiting." << std::endl;
    cancel();
  }
}

void Worker::restart_db_session() { start_db_session(dbconnection_string_); }

bool Worker::is_stream_alive(uint64_t serial) {
  return alive_streams_.contains(serial);
}

void Worker::add_stream(Stream *stream) {
  stream->serial_ = next_stream_serial_++;
  alive_streams_.insert(stream->serial_);
}

void Worker::remove_stream(Stream *stream) {
  alive_streams_.erase(stream->serial_);
}

} // namespace hm
