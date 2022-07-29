#pragma once

#include "httprouter.h"
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

struct ssl_ctx_st;

namespace hm {
class Worker;
class FileStream;
class HttpSession;
class HttpRequest;
class HttpResponse;

class Server {
  friend class Worker;
  friend class HttpSession;
  friend class Stream;

  using SSLContext =
      std::unique_ptr<struct ::ssl_ctx_st, void (*)(struct ::ssl_ctx_st *)>;

  inline static Server *instance_ = nullptr;

public:
  struct Config {
    const int num_threads;
    const char *port;
    const char *dhparam_file;
    const char *cert_file;
    const char *key_file;
  };

  Server(const Config &config);
  ~Server();

  void serve_static_files(std::string path_to_dir);

  void connect_database(const char *connection_string);

  Server &get(const char *route,
              std::invocable<HttpRequest *, HttpResponse *> auto &&cb);
  Server &post(const char *route,
               std::invocable<HttpRequest *, HttpResponse *> auto &&cb);

  void listen(double timeout = 0.0);

  friend Server *get_server() { return Server::instance_; }

private:
  void iterate_directory(std::string path);

  std::pair<int, std::optional<int>> start_listen();
  SSLContext create_ssl_ctx();

  static void acceptcb(struct ev_loop *loop, struct ev_io *t, int revents);
  static void timeoutcb(struct ev_loop *loop, struct ev_timer *t, int revents);

private:
  Config config_;

  size_t next_worker_ = 0;
  int listener_fd_ = -1;

  SSLContext ssl_ctx_;

  struct ev_loop *loop_;

  std::string static_root_;

  HttpRouter router_;
  std::vector<std::unique_ptr<Worker>> workers_;
};

Server &Server::get(const char *route,
                    std::invocable<HttpRequest *, HttpResponse *> auto &&cb) {
  router_.add_route(HttpMethod::GET, route, std::forward<decltype(cb)>(cb));
  return *this;
}

Server &Server::post(const char *route,
                     std::invocable<HttpRequest *, HttpResponse *> auto &&cb) {
  router_.add_route(HttpMethod::POST, route, std::forward<decltype(cb)>(cb));
  return *this;
}

} // namespace hm
