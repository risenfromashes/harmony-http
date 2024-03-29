#include <errno.h>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <postgresql/libpq-fe.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

#include <cassert>
#include <iostream>

namespace hm::util {

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

std::pair<string_map<std::string>, string_map<std::string>>
read_mime_types(const char *filename) {
  string_map<std::string> rt1, rt2;
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
        rt1.emplace(std::string(ext_start, ext_end),
                    std::string(std::begin(line), type_end));
        rt2.emplace(std::string(std::begin(line), type_end),
                    std::string(ext_start, ext_end));
      }
    }
  }
  // instead of whatever microsoft /etc/mime_types has
  rt1["ico"] = "image/x-icon";
  rt2["image/x-icon"] = "ico";
  return {rt1, rt2};
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

enum PGTypeOid {
  BOOL = 16,
  CHAR = 18,
  INT8 = 20,
  INT2 = 21,
  INT4 = 23,
  TEXT = 25,
  FLOAT4 = 700,
  FLOAT8 = 701,
  BPCHAR = 1042,
  VARCHAR = 1043,
  DATE = 1082,
  TIME = 1083,
  TIMESTAMP = 1114,
  TIMESTAMPTZ = 1184,
  NUMERIC = 1700
};

bool is_pg_string_type(Oid oid) {
  switch (oid) {
  case CHAR:
  case TEXT:
  case BPCHAR:
  case VARCHAR:
    return true;
  default:
    return false;
  }
}

bool is_pg_num_type(Oid oid) {
  switch (oid) {
  case INT2:
  case INT4:
  case INT8:
  case FLOAT4:
  case FLOAT8:
  case NUMERIC:
    return true;
  default:
    return false;
  }
}

void jsonify_row(PGresult *res, int row, std::string &out) {
  std::string &ret = out;
  int i = row;
  int n_cols = PQnfields(res);
  ret += "{";
  for (int j = 0; j < n_cols; j++) {
    Oid type = PQftype(res, j);
    char *field = PQfname(res, j);
    char *val = PQgetvalue(res, i, j);
    ret += "\"";
    ret += field;
    ret += "\"";
    ret += ":";
    if (PQgetisnull(res, i, j)) {
      ret += "null";
    } else if (val == nullptr) {
      ret += "null";
    } else if (type == BOOL) {
      ret += val[0] == 't' ? "true" : "false";
    } else if (is_pg_num_type(type)) {
      // numbers can be appended as is
      ret += val;
    } else { // otherwise assume string type
      // escape literal
      append_quoted_string(val, ret);
    }

    if (j < n_cols - 1) {
      ret += ',';
    }
  }
  ret += "}";
}

std::string to_json(PGresult *res) {
  switch (PQresultStatus(res)) {
  case PGRES_TUPLES_OK: {
    std::string ret;
    int n_rows = PQntuples(res);
    int n_cols = PQnfields(res);
    // rough estimate
    ret.reserve(n_rows * n_cols * 8);
    ret += "[";
    for (int i = 0; i < n_rows; i++) {
      jsonify_row(res, i, ret);
      if (i < n_rows - 1) {
        ret += ',';
      }
    }
    ret += "]";
    return ret;
  }
  case PGRES_SINGLE_TUPLE: {
    std::string ret;
    int n_cols = PQnfields(res);
    ret.reserve(n_cols * 8);
    jsonify_row(res, 0, ret);
    return ret;
  }
  case PGRES_COMMAND_OK:
    return "{}";
  default:
    // std::cerr << PQresultStatus(res) << std::endl;
    // std::cerr << PQresultErrorMessage(res) << std::endl;
    assert(false && "to_json shouldn't be called with given result type");
    break;
  }
  return "{}";
}

std::optional<std::string_view> get_cookie(std::string_view cookies,
                                           std::string_view key) {
  for (;;) {
    auto kbeg = cookies.find_first_not_of(' ');
    auto kend = cookies.find_first_of('=');
    auto k = cookies.substr(kbeg, kend);

    auto vbeg = kend + 1;
    auto vend = cookies.find_first_of(";", vbeg);
    auto v = cookies.substr(vbeg, vend);
    if (streq_l(k, key)) {
      return v;
    }
    if (vend == cookies.npos) {
      break;
    }
    cookies = cookies.substr(vend + 1);
  }
  return std::nullopt;
}

void append_quoted_string(std::string_view sv, std::string &ret_) {
  if (!std::any_of(sv.begin(), sv.end(), [](char c) {
        auto u = static_cast<unsigned char>(c);
        return c == '\\' || c == '"' || u < 0x20;
      })) {
    ret_ += '\"';
    ret_ += sv;
    ret_ += '\"';
    return;
  }

  ret_ += '\"';
  for (char c : sv) {
    switch (c) {
    case '\"':
      ret_ += "\\\"";
      break;
    case '\\':
      ret_ += "\\\\";
      break;
    case '\b':
      ret_ += "\\b";
      break;
    case '\f':
      ret_ += "\\f";
      break;
    case '\n':
      ret_ += "\\n";
      break;
    case '\r':
      ret_ += "\\r";
      break;
    case '\t':
      ret_ += "\\t";
      break;
    default: {
      auto u = static_cast<unsigned char>(c);
      if (u < 0x20) {
        const char hexchars[] = "0123456789abcdef";
        char hex[] = "\\u0000";
        hex[5] = hexchars[u & 0xF];
        u >>= 4;
        hex[4] = hexchars[u & 0xF];
        ret_ += hex;
      } else {
        ret_ += c;
      }
      break;
    }
    }
  }
  ret_ += '\"';
}

} // namespace hm::util
