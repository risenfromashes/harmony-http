#include <cstring>
#include <ev.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <openssl/types.h>

#include <nghttp2/nghttp2.h>

#include "filestream.h"
#include "httpsession.h"
#include "server.h"
#include "util.h"
#include "worker.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/decoder.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace hm {
Server::Server(const Config &config)
    : config_(config), ssl_ctx_(nullptr, nullptr) {
  loop_ = ev_default_loop(0);
  for (int i = 0; i < config_.num_threads; i++) {
    auto worker = std::make_unique<Worker>(this);
    workers_.push_back(std::move(worker));
  }
}

Server::~Server() { workers_.clear(); }

std::pair<int, std::optional<int>> Server::start_listen() {
  bool ok = false;
  addrinfo hints{};
  // both ipv4 and ipv6
  hints.ai_family = AF_UNSPEC;
  // TCP stream socket
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  addrinfo *res, *rp;
  // let address default to localhost and set port
  if (int r = getaddrinfo(nullptr, config_.port, &hints, &res); r != 0) {
    std::cerr << "getaddrinfo() failed: " << gai_strerror(r) << std::endl;
    freeaddrinfo(res);
    return {-1, std::nullopt};
  }

  for (rp = res; rp; rp = rp->ai_next) {
    int fd =
        socket(rp->ai_family, rp->ai_socktype | SOCK_NONBLOCK, rp->ai_protocol);
    if (fd == -1) {
      // failed to create socket
      continue;
    }
    // SET SO_REUSEADDR
    int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
                   (socklen_t)sizeof(val)) == -1) {
      close(fd);
      continue;
    }

    // set non-blocking
    // util::make_socket_nonblocking(fd);
    // socket ok. try to bind to address.
    if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0 && ::listen(fd, 1000) == 0) {
      // bind successful
      ok = true;
      freeaddrinfo(res);
      return {0, fd};
    } else {
      std::cerr << strerror(errno) << std::endl;
    }
    close(fd);
  }
  freeaddrinfo(res);
  return {-1, std::nullopt};
}

void Server::acceptcb(struct ev_loop *loop, struct ev_io *w, int revents) {
  auto self = static_cast<Server *>(w->data);
  for (;;) {
    auto fd = accept4(self->listener_fd_, nullptr, nullptr, SOCK_NONBLOCK);
    if (fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      std::cerr << "Accept failed with error: " << strerror(errno) << std::endl;
      std::cerr << "listener fd: " << self->listener_fd_ << std::endl;
      break;
    }
    self->workers_[self->next_worker_]->add_connection(fd);
    self->next_worker_ = (self->next_worker_ + 1) % self->config_.num_threads;
  }
}

Server::SSLContext Server::create_ssl_ctx() {

  auto ssl_ctx = util::unique_ptr<::SSL_CTX>(SSL_CTX_new(TLS_server_method()),
                                             SSL_CTX_free);

  if (!ssl_ctx) {
    std::cerr << "SSL setup failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  // https://www.openssl.org/docs/man3.0/man3/SSL_set_options.html
  auto ssl_opts = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                  SSL_OP_ENABLE_KTLS | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 |
                  SSL_OP_NO_COMPRESSION |
                  SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
                  SSL_OP_NO_TICKET | SSL_OP_CIPHER_SERVER_PREFERENCE;

  SSL_CTX_set_options(ssl_ctx.get(), ssl_opts);
  SSL_CTX_set_mode(ssl_ctx.get(), SSL_MODE_AUTO_RETRY);
  SSL_CTX_set_mode(ssl_ctx.get(), SSL_MODE_RELEASE_BUFFERS);

  // set cipher list
  if (SSL_CTX_set_cipher_list(ssl_ctx.get(), util::tls::CIPHER_LIST) == 0) {
    std::cerr << "SSL setup failed, couldn't configure ciphers: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  // set curves list
  if (SSL_CTX_set1_curves_list(ssl_ctx.get(), "P-256") != 1) {
    std::cerr << "SSL_CTX_set1_curves_list failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  // set dh params
  auto bio = util::unique_ptr<BIO>(BIO_new_file(config_.dhparam_file, "rb"),
                                   [](BIO *bio) { BIO_free(bio); });

  if (bio == nullptr) {
    std::cerr << "BIO_new_file() failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  EVP_PKEY *dh = nullptr;
  auto dctx = util::unique_ptr<OSSL_DECODER_CTX>(
      OSSL_DECODER_CTX_new_for_pkey(&dh, "PEM", nullptr, "DH",
                                    OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS,
                                    nullptr, nullptr),
      OSSL_DECODER_CTX_free);

  if (!OSSL_DECODER_from_bio(dctx.get(), bio.get())) {
    std::cerr << "OSSL_DECODER_from_bio() failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  if (SSL_CTX_set0_tmp_dh_pkey(ssl_ctx.get(), dh) != 1) {
    std::cerr << "SSL_CTX_set0_tmp_dh_pkey failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  // set private key file
  if (SSL_CTX_use_PrivateKey_file(ssl_ctx.get(), config_.key_file,
                                  SSL_FILETYPE_PEM) != 1) {
    std::cerr << "SSL_CTX_use_PrivateKey_file failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  // set certificate file
  if (SSL_CTX_use_certificate_chain_file(ssl_ctx.get(), config_.cert_file) !=
      1) {
    std::cerr << "SSL_CTX_use_certificate_chain_file failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  // check private key
  if (SSL_CTX_check_private_key(ssl_ctx.get()) != 1) {
    std::cerr << "SSL_CTX_check_private_key failed: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return {nullptr, nullptr};
  }

  // set next proto
  static struct {
    unsigned char list[256];
    size_t list_len;
  } next_proto;

  next_proto.list[0] = NGHTTP2_PROTO_VERSION_ID_LEN;
  std::memcpy(&next_proto.list[1], NGHTTP2_PROTO_VERSION_ID,
              NGHTTP2_PROTO_VERSION_ID_LEN);
  next_proto.list_len = 1 + NGHTTP2_PROTO_VERSION_ID_LEN;

  SSL_CTX_set_next_protos_advertised_cb(
      ssl_ctx.get(),
      [](SSL *ssl, const unsigned char **data, unsigned int *len, void *arg) {
        auto np = static_cast<decltype(next_proto) *>(arg);
        *data = np->list;
        *len = (unsigned int)np->list_len;
        return SSL_TLSEXT_ERR_OK;
      },
      &next_proto);

  SSL_CTX_set_alpn_select_cb(
      ssl_ctx.get(),
      [](SSL *ssl, const unsigned char **out, unsigned char *out_len,
         const unsigned char *in, unsigned int in_len, void *arg) {
        if (int rv = nghttp2_select_next_protocol((unsigned char **)out,
                                                  out_len, in, in_len);
            rv != 1) {
          return SSL_TLSEXT_ERR_NOACK;
        }
        return SSL_TLSEXT_ERR_OK;
      },
      &next_proto);

  return ssl_ctx;
}

void Server::timeoutcb(struct ev_loop *loop, struct ev_timer *w, int revents) {
  auto self = static_cast<Server *>(w->data);
  std::cerr << "Server timeout" << std::endl;
  ev_break(loop, EVBREAK_ALL);
}

static void configure_signals() {
  /* disable exit on sigpipe */
  struct sigaction sa {
    SIG_IGN
  };
  sigaction(SIGPIPE, &sa, NULL);
}

void Server::listen(double timeout) {
  configure_signals();

  ssl_ctx_ = create_ssl_ctx();

  if (!ssl_ctx_) {
    std::cerr << "Error: failed to create ssl context" << std::endl;
    return;
  }

  auto [err, fd] = start_listen();
  if (err) {
    std::cerr << "Error: failed to create server" << std::endl;
    return;
  }

  listener_fd_ = fd.value();

  for (int i = 0; i < config_.num_threads; i++) {
    workers_[i]->run();
  }

  bool enable_timeout = timeout > 0.0;

  ev_io accept_watcher;
  ev_io_init(&accept_watcher, acceptcb, listener_fd_, EV_READ);
  accept_watcher.data = this;

  ev_timer timer;
  if (enable_timeout) {
    ev_timer_init(&timer, timeoutcb, timeout, 0.0);
    ev_timer_start(loop_, &timer);
  }

  ev_io_start(loop_, &accept_watcher);
  ev_run(loop_, 0);
}

void Server::serve_static_files(std::string path) {
  while (path.back() == ' ') {
    path.pop_back();
  }

  if (path.back() != '/') {
    path += '/';
  }

  static_root_ = std::move(path);

  iterate_directory(static_root_);
}

void Server::connect_database(const char *connection_string) {
  for (int i = 0; i < config_.num_threads; i++) {
    workers_[i]->start_db_session(connection_string);
  }
}

void Server::iterate_directory(std::string path) {
  DIR *dir;
  dirent *de;

  if ((dir = opendir(path.c_str())) == nullptr) {
    std::cerr << "Couldn't open directory:" << path << ". No such directory"
              << std::endl;
    return;
  }

  while ((de = readdir(dir))) {
    if (de->d_name[0] == '.') {
      continue;
    }
    if (de->d_type & DT_DIR) {
      iterate_directory(path + de->d_name + "/");
    } else if (de->d_type & DT_REG | de->d_type & DT_LNK) {
      // add to all thread workers
      for (int i = 0; i < config_.num_threads; i++) {
        workers_[i]->add_static_file(path + de->d_name);
      }
    }
  }

  closedir(dir);
}

} // namespace hm
