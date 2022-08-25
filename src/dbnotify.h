#pragma once

#include <memory>
#include <string_view>

namespace hm::db {

class NotifyBase {
public:
  NotifyBase(void *ptr);

  NotifyBase(NotifyBase &&);
  NotifyBase &operator=(NotifyBase &&);

  NotifyBase(const NotifyBase &);
  NotifyBase &operator=(const NotifyBase &);

  ~NotifyBase();

  std::string_view channel() const;

  operator std::string_view() const;

  size_t length() const;

protected:
  void assign(const NotifyBase &b);
  void reset();
  static void deleter(void *ptr);

  void *pg_notify_;
  std::string_view payload_;
};

class Notify : public NotifyBase {
public:
  Notify(void *ptr);

  Notify(Notify &&);
  Notify &operator=(Notify &&);

  Notify(const Notify &) = delete;
  Notify &operator=(const Notify &) = delete;

  ~Notify();

private:
  std::unique_ptr<void, void (*)(void *)> ptr_;
};

class SharedNotify : public NotifyBase {
public:
  SharedNotify(void *ptr);

  SharedNotify(SharedNotify &&);
  SharedNotify &operator=(SharedNotify &&);

  SharedNotify(const SharedNotify &);
  SharedNotify &operator=(const SharedNotify &);

  ~SharedNotify();

private:
  std::shared_ptr<void> ptr_;
};

}; // namespace hm::db
