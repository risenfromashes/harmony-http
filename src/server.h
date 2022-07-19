#pragma once

#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

struct ssl_ctx_st;

namespace hm {
class HttpSession;
class Worker;
class FileStream;

class Server {
  friend class Worker;
  friend class HttpSession;
  friend class Stream;

  using SSLContext =
      std::unique_ptr<struct ::ssl_ctx_st, void (*)(struct ::ssl_ctx_st *)>;

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

  void listen(double timeout = 0.0);

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
  std::vector<std::unique_ptr<Worker>> workers_;
};
} // namespace hm
