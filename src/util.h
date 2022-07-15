#pragma once

#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <iostream>
#include <memory>
#include <ranges>
#include <vector>

#include <unordered_map>

namespace util {

template <class T> using unique_ptr = std::unique_ptr<T, void (*)(T *)>;

namespace tls {

constexpr char CIPHER_LIST[] =
    "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_"
    "SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-"
    "ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-"
    "CHACHA20-"
    "POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-"
    "AES256-GCM-SHA384";
}

class ScopeDebug {
public:
  ScopeDebug(const char *message);
  ~ScopeDebug();

private:
  const char *message_;
};

/* disable Nagle's algorithm */
int make_socket_nodelay(int fd);
/*  enable Nagle's algorithm */
int make_socket_delay(int fd);

int make_socket_nonblocking(int fd);

bool streq_l(const std::string_view &a, const std::string_view &b);
bool streq_l(const std::string_view &a, const std::string_view &b, size_t blen);

enum class HttpHeader {
  pAUTHORITY,
  pHOST,
  pMETHOD,
  pPATH,
  pPROTOCOL,
  pSCHEME,
  pSTATUS,
  ACCEPT_ENCODING,
  ACCEPT_LANGUAGE,
  ALT_SVC,
  CACHE_CONTROL,
  CONNECTION,
  CONTENT_LENGTH,
  CONTENT_TYPE,
  COOKIE,
  DATE,
  EARLY_DATA,
  EXPECT,
  FORWARDED,
  HOST,
  HTTP2_SETTINGS,
  IF_MODIFIED_SINCE,
  KEEP_ALIVE,
  LINK,
  LOCATION,
  PROXY_CONNECTION,
  SEC_WEBSOCKET_ACCEPT,
  SEC_WEBSOCKET_KEY,
  SERVER,
  TE,
  TRAILER,
  TRANSFER_ENCODING,
  UPGRADE,
  USER_AGENT,
  VIA,
  X_FORWARDED_FOR,
  X_FORWARDED_PROTO,
  MAXIDX,
  eNOTFOUND,
};

HttpHeader lookup_header(std::string_view name);
HttpHeader lookup_header(const uint8_t *name_, size_t namelen);

inline nghttp2_nv make_nv(std::string_view n, std::string_view v,
                          std::pair<bool, bool> copy = {false, false},
                          bool never_indexed = false) {
  return {.name = (uint8_t *)n.data(),
          .value = (uint8_t *)v.data(),
          .namelen = n.size(),
          .valuelen = v.size(),
          .flags = (uint8_t)((never_indexed * NGHTTP2_NV_FLAG_NO_INDEX) |
                             ((!copy.first) * NGHTTP2_NV_FLAG_NO_COPY_NAME) |
                             ((!copy.second) * NGHTTP2_NV_FLAG_NO_COPY_VALUE))};
}

// Initially keeps a stack allocated buffer
template <size_t N> class MemBlock {
public:
  MemBlock() {}
  ~MemBlock() {
    for (int i = 0; i < add_.size(); i++) {
      free(add_[i]);
    }
  }

  void *alloc(size_t n) {
    // ScopeDebug sc("MemBlock::alloc");

    // std::cerr << "requested alloc size: " << n << std::endl;
    if (n + pos <= N) {
      void *p = data_.data() + pos;
      pos += n;
      // std::cerr << "memory dump: " << std::endl;
      // std::cerr << std::string_view((char *)data_.data(), pos) << std::endl;
      return p;
    }
    // std::cerr << "dynamic allocating" << std::endl;
    auto rt = malloc(n);
    add_.push_back(rt);
    return rt;
  }

private:
  size_t pos = 0;
  std::array<uint8_t, N> data_;
  // additional memory
  std::vector<void *> add_;
};

// str must be nullterminated
time_t parse_http_date(const char *str);

// out must own |src.size() * 3 + 1| bytes of memory
std::string_view percent_decode(const std::string_view &src, char *out);
std::string percent_decode(const std::string_view &src);

template <size_t N>
std::string_view percent_decode(const std::string_view &src,
                                MemBlock<N> &memblock) {
  char *out = static_cast<char *>(memblock.alloc(src.size() + 1));
  return percent_decode(src, out);
}

/* Conditional logic w/ lookup tables to check if id is banned */
bool check_http2_cipher_block_list(SSL *ssl);

inline bool check_http2_requirements(SSL *ssl) {
  return SSL_version(ssl) >= TLS1_2_VERSION &&
         !check_http2_cipher_block_list(ssl);
}

inline bool check_h2_is_selected(const std::string_view &proto) {
  return proto == "h2" || proto == "h2-14" || proto == "h2-16";
}

template <size_t N>
inline std::string_view to_string(int64_t n, MemBlock<N> &memblock) {
  size_t size = std::log10((double)n) + 3;
  char *out = static_cast<char *>(memblock.alloc(size));
  std::snprintf(out, size, "%li", n);
  return out;
}

template <size_t N>
inline std::string_view to_string(uint64_t n, MemBlock<N> &memblock) {
  size_t size = std::log10((double)n) + 3;
  char *out = static_cast<char *>(memblock.alloc(size));
  std::snprintf(out, size, "%lu", n);
  return out;
}

inline std::string_view to_string_view(nghttp2_rcbuf *rcbuf) {
  auto buf = nghttp2_rcbuf_get_buf(rcbuf);
  return std::string_view{reinterpret_cast<const char *>(buf.base), buf.len};
}

inline void hexdump8(FILE *out, const uint8_t *first, const uint8_t *last) {
  auto stop = std::min(first + 8, last);
  for (auto k = first; k != stop; ++k) {
    fprintf(out, "%02x ", *k);
  }
  // each byte needs 3 spaces (2 hex value and space)
  for (; stop != first + 8; ++stop) {
    fputs("   ", out);
  }
  // we have extra space after 8 bytes
  fputc(' ', out);
}

inline void hexdump(FILE *out, const uint8_t *src, size_t len) {
  if (len == 0) {
    return;
  }
  size_t buflen = 0;
  auto repeated = false;
  std::array<uint8_t, 16> buf{};
  auto end = src + len;
  auto i = src;
  for (;;) {
    auto nextlen =
        std::min(static_cast<size_t>(16), static_cast<size_t>(end - i));
    if (nextlen == buflen &&
        std::equal(std::begin(buf), std::begin(buf) + buflen, i)) {
      // as long as adjacent 16 bytes block are the same, we just
      // print single '*'.
      if (!repeated) {
        repeated = true;
        fputs("*\n", out);
      }
      i += nextlen;
      continue;
    }
    repeated = false;
    fprintf(out, "%08lx", static_cast<unsigned long>(i - src));
    if (i == end) {
      fputc('\n', out);
      break;
    }
    fputs("  ", out);
    hexdump8(out, i, end);
    hexdump8(out, i + 8, std::max(i + 8, end));
    fputc('|', out);
    auto stop = std::min(i + 16, end);
    buflen = stop - i;
    auto p = buf.data();
    for (; i != stop; ++i) {
      *p++ = *i;
      if (0x20 <= *i && *i <= 0x7e) {
        fputc(*i, out);
      } else {
        fputc('.', out);
      }
    }
    fputs("|\n", out);
  }
}

std::unordered_map<std::string, std::string>
read_mime_types(const char *filename = "/etc/mime.types");

std::string http_date(time_t t);
char *http_date(time_t t, char *res);

template <size_t N>
inline std::string_view http_date(time_t t, MemBlock<N> &memblock) {
  char *out = static_cast<char *>(memblock.alloc(29));
  out[0] = '\0';
  return {http_date(t, out), 29};
}

inline std::string http_date(time_t t) {
  /* Sat, 27 Sep 2014 06:31:15 GMT */
  std::string res(29, 0);
  http_date(t, &res[0]);
  return res;
}

} // namespace util
