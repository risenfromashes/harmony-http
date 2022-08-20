#include "json.h"

namespace hm::json {

Node::operator Array() && { return Array(ret_); }
Node::operator Object() && { return Object(ret_); }

Node Writer::root() { return Node(ret_); }
std::string Writer::string() {
  if (ret_.back() == ',') {
    ret_.pop_back();
  }
  return std::move(ret_);
}

Array::Array(std::string &ret) : ret_(ret) { ret_ += '['; }
Array::~Array() {
  if (ret_.back() == ',') {
    ret_.back() = ']';
    ret_ += ',';
  } else {
    ret_ += "],";
  }
}

Object::Object(std::string &ret) : ret_(ret) { ret_ += '{'; }
Object::~Object() {
  if (ret_.back() == ',') {
    ret_.back() = '}';
    ret_ += ',';
  } else {
    ret_ += "},";
  }
}

Node Object::operator[](std::string_view key) {
  ret_ += "\"";
  ret_ += key;
  ret_ += "\":";
  return Node(ret_);
}

void Node::append_quoted_string(std::string_view sv) {
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
} // namespace hm::json
