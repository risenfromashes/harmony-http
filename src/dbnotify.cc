#include "dbnotify.h"

#include <libpq-fe.h>

namespace hm::db {

#define pg_notify static_cast<PGnotify *>(pg_notify_)

Notify::Notify(void *ptr) : pg_notify_(ptr), payload_(pg_notify->extra) {}
Notify::~Notify() { PQfreemem(pg_notify); }

std::string_view Notify::channel() { return pg_notify->relname; }

Notify::operator std::string_view() { return payload_; }

size_t Notify::length() { return payload_.length(); }

}; // namespace hm::db
