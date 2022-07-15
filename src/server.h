#pragma once

#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

class HttpSession;
class Worker;
class FileStream;

class Server {
  friend class Worker;
  friend class HttpSession;
  friend class Stream;

  using SSLContext =
      std::unique_ptr<struct ssl_ctx_st, void (*)(struct ssl_ctx_st *)>;

public:
  Server(int num_workers);
  ~Server();

  void listen(const char *port, double timeout = 0.0);

  void serve_static_files(std::string path_to_dir);

private:
  void iterate_directory(std::string path_to_dir);
  FileStream *get_static_file(std::string_view path);

  static std::pair<int, std::optional<int>> start_listen(const char *port);
  static SSLContext create_ssl_ctx();
  static void acceptcb(struct ev_loop *loop, struct ev_io *t, int revents);
  static void timeoutcb(struct ev_loop *loop, struct ev_timer *t, int revents);

private:
  int num_workers_;
  size_t next_worker_ = 0;
  int listener_fd_ = -1;

  SSLContext ssl_ctx_;

  struct ev_loop *loop_;

  std::string static_root_;
  std::vector<std::unique_ptr<Worker>> workers_;
  /* no reference invalidation */
  std::unordered_map<std::string_view, std::unique_ptr<FileStream>>
      static_files_;
};
