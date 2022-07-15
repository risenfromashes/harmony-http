#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

#include <iostream>

namespace util {

ScopeDebug::ScopeDebug(const char *m) : message_(m) {
  std::cerr << "[ScopeDebug] "
            << "Entered: " << m << std::endl;
}
ScopeDebug::~ScopeDebug() {
  std::cerr << "[ScopeDebug] "
            << "Leaving: " << message_ << std::endl;
}

int make_socket_nodelay(int fd) {
  int val = 1;
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&val),
                 sizeof(val)) == -1) {
    return -1;
  }
  return 0;
}

int make_socket_delay(int fd) {
  int val = 0;
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&val),
                 sizeof(val)) == -1) {
    return -1;
  }
  return 0;
}

int make_socket_nonblocking(int fd) {
  int flags, rv;
  while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR)
    ;
  while ((rv = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR)
    ;
  return rv;
}

inline bool is_alpha(const char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

inline bool is_lower(const char c) { return 'a' <= c && c <= 'z'; }
inline bool is_upper(const char c) { return 'A' <= c && c <= 'Z'; }

inline bool is_digit(const char c) { return '0' <= c && c <= '9'; }

inline bool is_hex_digit(const char c) {
  return is_digit(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

inline char to_lower(const char c) {
  return is_alpha(c) ? (is_upper(c) ? c - 'A' + 'a' : c) : c;
}

bool streq_l(const std::string_view &a, const std::string_view &b) {
  if (a.size() != b.size()) {
    return false;
  }
  auto len = a.size();
  for (size_t i = 0; i < len; i++) {
    if (to_lower(a[i]) != to_lower(b[i])) {
      return false;
    }
  }
  return true;
}

bool streq_l(const std::string_view &a, const std::string_view &b,
             size_t blen) {
  return streq_l(a, {b.data(), blen});
}

HttpHeader lookup_header(std::string_view name) {
  using enum HttpHeader;
  switch (name.size()) {
  case 2:
    switch (name[1]) {
    case 'e':
      if (streq_l("t", name, 1)) {
        return TE;
      }
      break;
    }
    break;
  case 3:
    switch (name[2]) {
    case 'a':
      if (util::streq_l("vi", name, 2)) {
        return VIA;
      }
      break;
    }
    break;
  case 4:
    switch (name[3]) {
    case 'e':
      if (util::streq_l("dat", name, 3)) {
        return DATE;
      }
      break;
    case 'k':
      if (util::streq_l("lin", name, 3)) {
        return LINK;
      }
      break;
    case 't':
      if (util::streq_l("hos", name, 3)) {
        return HOST;
      }
      break;
    }
    break;
  case 5:
    switch (name[4]) {
    case 'h':
      if (util::streq_l(":pat", name, 4)) {
        return pPATH;
      }
      break;
    case 't':
      if (util::streq_l(":hos", name, 4)) {
        return HOST;
      }
      break;
    }
    break;
  case 6:
    switch (name[5]) {
    case 'e':
      if (util::streq_l("cooki", name, 5)) {
        return COOKIE;
      }
      break;
    case 'r':
      if (util::streq_l("serve", name, 5)) {
        return SERVER;
      }
      break;
    case 't':
      if (util::streq_l("expec", name, 5)) {
        return EXPECT;
      }
      break;
    }
    break;
  case 7:
    switch (name[6]) {
    case 'c':
      if (util::streq_l("alt-sv", name, 6)) {
        return ALT_SVC;
      }
      break;
    case 'd':
      if (util::streq_l(":metho", name, 6)) {
        return pMETHOD;
      }
      break;
    case 'e':
      if (util::streq_l(":schem", name, 6)) {
        return pSCHEME;
      }
      if (util::streq_l("upgrad", name, 6)) {
        return UPGRADE;
      }
      break;
    case 'r':
      if (util::streq_l("traile", name, 6)) {
        return TRAILER;
      }
      break;
    case 's':
      if (util::streq_l(":statu", name, 6)) {
        return pSTATUS;
      }
      break;
    }
    break;
  case 8:
    switch (name[7]) {
    case 'n':
      if (util::streq_l("locatio", name, 7)) {
        return LOCATION;
      }
      break;
    }
    break;
  case 9:
    switch (name[8]) {
    case 'd':
      if (util::streq_l("forwarde", name, 8)) {
        return FORWARDED;
      }
      break;
    case 'l':
      if (util::streq_l(":protoco", name, 8)) {
        return pPROTOCOL;
      }
      break;
    }
    break;
  case 10:
    switch (name[9]) {
    case 'a':
      if (util::streq_l("early-dat", name, 9)) {
        return EARLY_DATA;
      }
      break;
    case 'e':
      if (util::streq_l("keep-aliv", name, 9)) {
        return KEEP_ALIVE;
      }
      break;
    case 'n':
      if (util::streq_l("connectio", name, 9)) {
        return CONNECTION;
      }
      break;
    case 't':
      if (util::streq_l("user-agen", name, 9)) {
        return USER_AGENT;
      }
      break;
    case 'y':
      if (util::streq_l(":authorit", name, 9)) {
        return pAUTHORITY;
      }
      break;
    }
    break;
  case 12:
    switch (name[11]) {
    case 'e':
      if (util::streq_l("content-typ", name, 11)) {
        return CONTENT_TYPE;
      }
      break;
    }
    break;
  case 13:
    switch (name[12]) {
    case 'l':
      if (util::streq_l("cache-contro", name, 12)) {
        return CACHE_CONTROL;
      }
      break;
    }
    break;
  case 14:
    switch (name[13]) {
    case 'h':
      if (util::streq_l("content-lengt", name, 13)) {
        return CONTENT_LENGTH;
      }
      break;
    case 's':
      if (util::streq_l("http2-setting", name, 13)) {
        return HTTP2_SETTINGS;
      }
      break;
    }
    break;
  case 15:
    switch (name[14]) {
    case 'e':
      if (util::streq_l("accept-languag", name, 14)) {
        return ACCEPT_LANGUAGE;
      }
      break;
    case 'g':
      if (util::streq_l("accept-encodin", name, 14)) {
        return ACCEPT_ENCODING;
      }
      break;
    case 'r':
      if (util::streq_l("x-forwarded-fo", name, 14)) {
        return X_FORWARDED_FOR;
      }
      break;
    }
    break;
  case 16:
    switch (name[15]) {
    case 'n':
      if (util::streq_l("proxy-connectio", name, 15)) {
        return PROXY_CONNECTION;
      }
      break;
    }
    break;
  case 17:
    switch (name[16]) {
    case 'e':
      if (util::streq_l("if-modified-sinc", name, 16)) {
        return IF_MODIFIED_SINCE;
      }
      break;
    case 'g':
      if (util::streq_l("transfer-encodin", name, 16)) {
        return TRANSFER_ENCODING;
      }
      break;
    case 'o':
      if (util::streq_l("x-forwarded-prot", name, 16)) {
        return X_FORWARDED_PROTO;
      }
      break;
    case 'y':
      if (util::streq_l("sec-websocket-ke", name, 16)) {
        return SEC_WEBSOCKET_KEY;
      }
      break;
    }
    break;
  case 20:
    switch (name[19]) {
    case 't':
      if (util::streq_l("sec-websocket-accep", name, 19)) {
        return SEC_WEBSOCKET_ACCEPT;
      }
      break;
    }
    break;
  }
  return eNOTFOUND;
}

HttpHeader lookup_header(const uint8_t *name, size_t namelen) {
  return lookup_header({reinterpret_cast<const char *>(name), namelen});
}

static int count_leap_year(int y) {
  y--;
  return y / 4 - y / 100 + y / 400;
}

static bool is_leap_year(int y) {
  return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

time_t parse_http_date(const char *str) {
  static int daysum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  tm tm{};
  char *r = strptime(str, "%a, %d %b %Y %H:%M:%S GMT", &tm);
  if (r == 0) {
    return 0;
  }
  int64_t t;
  if (tm.tm_mon > 11) {
    return -1;
  }

  int num_leap_year =
      count_leap_year(tm.tm_year + 1900) - count_leap_year(1970);
  int days = (tm.tm_year - 70) * 365 + num_leap_year + daysum[tm.tm_mon] +
             tm.tm_mday - 1;

  if (tm.tm_mon >= 2 && is_leap_year(tm.tm_year + 1900)) {
    ++days;
  }
  t = (days * 24 + tm.tm_hour) * 3600LL + tm.tm_min * 60 + tm.tm_sec;

  return (time_t)t;
}

inline char from_hex(const char c) {
  if (c <= '9') {
    return c - '0';
  }
  if (c <= 'Z') {
    return c - 'A' + 10;
  }
  if (c <= 'z') {
    return c - 'a' + 10;
  }
  return 0;
}

std::string_view percent_decode(const std::string_view &src, char *out) {
  char *p = out;
  for (auto c = src.begin(), e = src.end(); c != e; ++c) {
    if (*c == '%') {
      if (c + 1 != e && c + 2 != e) {
        *p++ = from_hex(*(c + 1)) << 4 | from_hex(*(c + 2));
        c += 2;
      }
    } else if (*c == '+') {
      *p++ = ' ';
    } else {
      *p++ = *c;
    }
  }
  *p = '\0';
  return {out, (size_t)(p - out)};
}

std::string percent_decode(const std::string_view &src) {
  std::string str;
  for (auto c = src.begin(), e = src.end(); c != e; ++c) {
    if (*c == '%') {
      if (c + 1 != e && c + 2 != e) {
        str += from_hex(*(c + 1)) << 4 | from_hex(*(c + 2));
        c += 2;
      }
    } else if (*c == '+') {
      str += ' ';
    } else {
      str += *c;
    }
  }
  return str;
}

/* Conditional logic w/ lookup tables to check if id is banned */
#define IS_CIPHER_BANNED_METHOD2(id)                                           \
  ((0x0000 <= id && id <= 0x00FF &&                                            \
    "\xFF\xFF\xFF\xCF\xFF\xFF\xFF\xFF\x7F\x00\x00\x00\x80\x3F\x00\x00"         \
    "\xF0\xFF\xFF\x3F\xF3\xF3\xFF\xFF\x3F\x00\x00\x00\x00\x00\x00\x80"         \
            [(id & 0xFF) / 8] &                                                \
        (1 << (id % 8))) ||                                                    \
   (0xC000 <= id && id <= 0xC0FF &&                                            \
    "\xFE\xFF\xFF\xFF\xFF\x67\xFE\xFF\xFF\xFF\x33\xCF\xFC\xCF\xFF\xCF"         \
    "\x3C\xF3\xFC\x3F\x33\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"         \
            [(id & 0xFF) / 8] &                                                \
        (1 << (id % 8)))) &                                                    \
      0xFFFFFF;

bool check_http2_cipher_block_list(SSL *ssl) {
  int id = SSL_CIPHER_get_id(SSL_get_current_cipher(ssl));
  return IS_CIPHER_BANNED_METHOD2(id);
}

#undef IS_CIPHER_BANNED_METHOD2

std::unordered_map<std::string, std::string>
read_mime_types(const char *filename) {

  std::unordered_map<std::string, std::string> rt;
  std::ifstream in(filename);
  if (in) {
    std::string line;
    auto delim_pred = [](char c) { return c == ' ' || c == '\t'; };

    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }

      auto type_end = std::ranges::find_if(line, delim_pred);
      if (type_end == std::begin(line)) {
        continue;
      }

      auto ext_end = type_end;
      for (;;) {
        auto ext_start = std::find_if_not(ext_end, std::end(line), delim_pred);
        if (ext_start == std::end(line)) {
          break;
        }
        ext_end = std::find_if(ext_start, std::end(line), delim_pred);
        rt.emplace(std::string(ext_start, ext_end),
                   std::string(std::begin(line), type_end));
      }
    }
  }
  return rt;
}

/* writes zero padded uint32_t to out */
inline char *write_uint(uint32_t n, char *out, size_t len) {
  // write number, start from msb
  char *p = out + len - 1;
  do {
    *p-- = (n % 10) + '0';
    n /= 10;
  } while (p >= out);

  return out + len;
}

constexpr const char *MONTH[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr const char *DAY_OF_WEEK[] = {"Sun", "Mon", "Tue", "Wed",
                                       "Thu", "Fri", "Sat"};

char *http_date(time_t t, char *res) {
  struct tm tms;

  if (gmtime_r(&t, &tms) == nullptr) {
    return res;
  }

  auto p = res;

  auto s = DAY_OF_WEEK[tms.tm_wday];
  p = std::copy_n(s, 3, p);
  *p++ = ',';
  *p++ = ' ';
  p = write_uint(tms.tm_mday, p, 2);
  *p++ = ' ';
  s = MONTH[tms.tm_mon];
  p = std::copy_n(s, 3, p);
  *p++ = ' ';
  p = write_uint(tms.tm_year + 1900, p, 4);
  *p++ = ' ';
  p = write_uint(tms.tm_hour, p, 2);
  *p++ = ':';
  p = write_uint(tms.tm_min, p, 2);
  *p++ = ':';
  p = write_uint(tms.tm_sec, p, 2);
  s = " GMT";
  p = std::copy_n(s, 4, p);

  return res;
}

} // namespace util
