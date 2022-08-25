#include "dbnotify.h"

#include <iostream>
#include <libpq-fe.h>

namespace hm::db {

#define pg_notify static_cast<PGnotify *>(pg_notify_)

NotifyBase::NotifyBase(void *ptr)
    : pg_notify_(ptr), payload_(pg_notify->extra) {}

NotifyBase::~NotifyBase() {}

void NotifyBase::deleter(void *ptr) { PQfreemem(ptr); }

void NotifyBase::assign(const NotifyBase &b) {
  pg_notify_ = b.pg_notify_;
  payload_ = b.payload_;
}

void NotifyBase::reset() {
  pg_notify_ = nullptr;
  payload_ = "";
}

NotifyBase::NotifyBase(NotifyBase &&b) {
  assign(b);
  b.reset();
}

NotifyBase &NotifyBase::operator=(NotifyBase &&b) {
  assign(b);
  b.reset();
  return *this;
}

NotifyBase::NotifyBase(const NotifyBase &b) { assign(b); }

NotifyBase &NotifyBase::operator=(const NotifyBase &b) {
  assign(b);
  return *this;
}

std::string_view NotifyBase::channel() const { return pg_notify->relname; }

NotifyBase::operator std::string_view() const { return payload_; }

size_t NotifyBase::length() const { return payload_.length(); }

Notify::Notify(void *ptr) : ptr_(ptr, NotifyBase::deleter), NotifyBase(ptr) {}

Notify::Notify(Notify &&b)
    : ptr_(std::move(b.ptr_)), NotifyBase(std::move(b)) {}

Notify &Notify::operator=(Notify &&b) {
  assign(b);
  b.reset();
  ptr_ = std::move(b.ptr_);
  return *this;
}

Notify::~Notify() {}

SharedNotify::SharedNotify(void *ptr)
    : ptr_(ptr, NotifyBase::deleter), NotifyBase(ptr) {}

SharedNotify::SharedNotify(SharedNotify &&b)
    : ptr_(std::move(b.ptr_)), NotifyBase(std::move(b)) {}

SharedNotify &SharedNotify::operator=(SharedNotify &&b) {
  assign(b);
  b.reset();
  ptr_ = std::move(b.ptr_);
  return *this;
}

SharedNotify::SharedNotify(const SharedNotify &b)
    : ptr_(b.ptr_), NotifyBase(b) {}

SharedNotify &SharedNotify::operator=(const SharedNotify &b) {
  assign(b);
  ptr_ = b.ptr_;
  return *this;
}

SharedNotify::~SharedNotify() {}

}; // namespace hm::db
