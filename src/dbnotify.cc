#include "dbnotify.h"

#include <libpq-fe.h>

namespace hm::db {

#define pg_notify static_cast<PGnotify *>(pg_notify_)

Notify::Notify(void *ptr) : pg_notify_(ptr), payload_(pg_notify->extra) {}

Notify::~Notify() {
  if (pg_notify_) {
    PQfreemem(pg_notify);
  }
}

Notify::Notify(Notify &&b) {
  pg_notify_ = b.pg_notify_;
  payload_ = b.payload_;
  b.pg_notify_ = nullptr;
  b.payload_ = "";
}

Notify &Notify::operator=(Notify &&b) {
  if (pg_notify_) {
    PQfreemem(pg_notify_);
  }
  pg_notify_ = b.pg_notify_;
  payload_ = b.payload_;
  b.pg_notify_ = nullptr;
  b.payload_ = "";
  return *this;
}

std::string_view Notify::channel() { return pg_notify->relname; }

Notify::operator std::string_view() { return payload_; }

size_t Notify::length() { return payload_.length(); }

}; // namespace hm::db
