#pragma once

#include <concepts>
#include <coroutine>
#include <optional>

// basic coroutine resumable task
namespace hm {

template <class T = void> struct Promise;
template <class T = void> struct Awaitable;

template <class T = void> class Task {
public:
  using promise_type = Promise<T>;

  using handle_type = std::coroutine_handle<promise_type>;

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
    if (handle_ && !handle_.done()) {
      handle_.resume();
    }
  }

  void
  set_result(std::convertible_to<T> auto &&v) requires(!std::same_as<T, void>) {
    if (handle_ && !handle_.done()) {
      handle_.promise().set_value(std::forward<decltype(v)>(v));
    }
  }

  std::optional<T> get_result() requires(!std::same_as<T, void>) {
    if (handle_ && handle_.done()) {
      return handle_.promise().value_;
    }
    return std::nullopt;
  }

  ~Task() {
    if (handle_) {
      handle_.destroy();
    }
  }

  auto operator co_await();

private:
  handle_type handle_;
};

template <class T> struct Promise {
  using handle_type = std::coroutine_handle<Promise<T>>;

  T value_;

  auto initial_suspend() noexcept {
    // not lazy start immediately
    return std::suspend_never{};
  }

  auto final_suspend() noexcept {
    // always suspend let destructor of task destroy coroutine
    return std::suspend_always{};
  }

  void unhandled_exception() {}

  Task<T> get_return_object() {
    // return task to caller
    return handle_type::from_promise(*this);
  }

  Task<T> get_task() { return get_return_object(); }

  void set_value(std::convertible_to<T> auto &&v) {
    value_ = std::forward<decltype(v)>(v);
  }

  void return_value(std::convertible_to<T> auto &&v) {
    set_value(std::forward<decltype(v)>(v));
  }
};

template <> struct Promise<void> {
  using handle_type = std::coroutine_handle<Promise<>>;

  auto initial_suspend() noexcept {
    // not lazy start immediately
    return std::suspend_never{};
  }

  auto final_suspend() noexcept {
    // always suspend let destructor of task destroy coroutine
    return std::suspend_always{};
  }

  void unhandled_exception() {}

  Task<> get_return_object() {
    // return task to caller
    return handle_type::from_promise(*this);
  }

  Task<> get_task() { return get_return_object(); }

  void return_void() {}
};

template <class T> struct Awaitable {
  using promise_type = Promise<T>;
  using handle_type = std::coroutine_handle<Promise<T>>;

  promise_type *promise_;

  bool await_ready() { return false; }

  void await_suspend(handle_type handle) { promise_ = &handle.promise(); }

  T await_resume() { return promise_->value_; }
};

template <> struct Awaitable<void> {
  using promise_type = Promise<>;
  using handle_type = std::coroutine_handle<Promise<>>;

  bool await_ready() { return false; }

  void await_suspend(handle_type handle) {}

  void await_resume() {}
};

template <class T> auto Task<T>::operator co_await() { return Awaitable(); }

} // namespace hm
