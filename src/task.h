#pragma once

#include <concepts>
#include <coroutine>
#include <optional>

// basic coroutine resumable task
namespace hm {

template <class T = void> class Task {
public:
  struct PromiseBase;
  struct Promise;

  using promise_type = Promise;
  using handle_type = std::coroutine_handle<Promise>;

  // empty task
  Task() : handle_(nullptr) {}
  Task(handle_type handle) : handle_(handle) {}
  Task(Task &&other) : handle_(other.handle_) { other.handle_ = nullptr; }

  Task &operator=(Task &&other) {
    if (handle_) {
      handle_.destroy();
    }
    handle_ = other.handle_;
    other.handle_ = nullptr;
    return *this;
  }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;

  operator bool() { return handle_; }

  void resume() {
    assert(handle_ && !handle_.done());
    handle_.resume();
  }

  bool done() { handle_.done(); }

  void
  set_result(std::convertible_to<T> auto &&v) requires(!std::same_as<T, void>);

  T get_result() requires(!std::same_as<T, void>);

  ~Task() {
    if (handle_) {
      handle_.destroy();
    }
  }

private:
  handle_type handle_;
};

template <class T> struct Task<T>::PromiseBase {
  auto initial_suspend() noexcept { return std::suspend_never{}; }
  // always suspend; let destructor destroy coroutine
  auto final_suspend() noexcept { return std::suspend_always{}; }

  void unhandled_exception() {}

  Task<T> get_return_object() {
    return handle_type::from_promise(*(static_cast<Promise *>(this)));
  }

  Task<T> get_task() { return get_return_object(); }
};

template <class T> struct Task<T>::Promise : public PromiseBase {
  void return_value(std::convertible_to<T> auto &&v) {
    value = std::forward<decltype(v)>(v);
  }
  T value;
};

template <> struct Task<void>::Promise : public PromiseBase {
  void return_void() {}
};

template <class T>
void Task<T>::set_result(std::convertible_to<T> auto &&v) requires(
    !std::same_as<T, void>) {
  assert(handle_ && !handle_.done());
  handle_.promise().value = std::forward<decltype(v)>(v);
}

template <class T> T Task<T>::get_result() requires(!std::same_as<T, void>) {
  assert(handle_ && handle_.done());
  return handle_.promise().value;
}

} // namespace hm
