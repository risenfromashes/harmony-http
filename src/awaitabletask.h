#pragma once
#include <cassert>
#include <coroutine>
#include <optional>

#include <iostream>

#include "coro.h"

namespace hm {

struct keep_value {};

template <class T = void> class AwaitableTask {
public:
  struct PromiseBase;
  struct Promise;
  struct Awaitable;

  using promise_type = Promise;
  using handle_type = std::coroutine_handle<promise_type>;

  AwaitableTask(handle_type handle) : handle_(handle) {}
  AwaitableTask(AwaitableTask &&other) : handle_(other.handle_) {
    other.handle_ = nullptr;
  }
  AwaitableTask &operator=(AwaitableTask &&other) {
    if (handle_) {
      handle_.destroy();
    }
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }

  AwaitableTask(const AwaitableTask &) = delete;
  AwaitableTask &operator=(const AwaitableTask &) = delete;

  ~AwaitableTask() {
    if (handle_) {
      handle_.destroy();
    }
  }

  bool done() {
    assert(handle_);
    return handle_.done();
  }

  void resume() {
    assert(handle_ && !handle_.done());
    handle_.resume();
  }

  void
  set_result(std::convertible_to<T> auto &&v) requires(!std::same_as<T, void>) {
    assert(handle_ && !handle_.done());
    handle_.promise().return_value(std::forward<decltype(v)>(v));
  }

  auto operator co_await() &&;

private:
  Promise &promise() { return handle_.promise(); }

private:
  handle_type handle_;
};

template <class T> struct AwaitableTask<T>::PromiseBase {

  using handle_type = std::coroutine_handle<Promise>;

  std::coroutine_handle<> continuation = nullptr;

  void set_continuation(std::coroutine_handle<> cont) { continuation = cont; }

  auto initial_suspend() noexcept { return std::suspend_never{}; }

  auto final_suspend() noexcept { return SuspendAndContinue(continuation); }

  void unhandled_exception() {}

  AwaitableTask<T> get_return_object() {
    return handle_type::from_promise(*(static_cast<Promise *>(this)));
  }

  AwaitableTask<T> get_task() { return get_return_object(); }
};

template <class T> struct AwaitableTask<T>::Promise : public PromiseBase {
  void return_value(std::convertible_to<T> auto &&v) {
    value = std::forward<decltype(v)>(v);
  }
  // keep existing value
  void return_value(keep_value) {}

  std::optional<T> value;
};

template <> struct AwaitableTask<void>::Promise : public PromiseBase {
  void return_void() {}
};

template <class T> struct AwaitableTask<T>::Awaitable {
  bool await_ready() { return task.done(); }

  void await_suspend(std::coroutine_handle<> handle) {
    task.promise().continuation = handle;
  }

  T await_resume() {
    if constexpr (!std::same_as<T, void>) {
      assert(task.promise().value.has_value());
      return std::move(task.promise().value.value());
    }
  }

  Awaitable(const Awaitable &) = delete;
  Awaitable &operator=(const Awaitable &) = delete;

  Awaitable(AwaitableTask<T> &&t) : task(std::move(t)) {}
  ~Awaitable() {}

  AwaitableTask<T> task;
};

// When the task is awaited, the current coroutine is saved for continuation
template <class T> auto AwaitableTask<T>::operator co_await() && {
  return Awaitable(std::move(*this));
}

} // namespace hm
