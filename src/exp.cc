#include <concepts>
#include <coroutine>
#include <cstdio>
#include <iostream>
#include <optional>

// basic coroutine resumable task
namespace hm {

template <class T = void> struct Promise;
template <class T = void> struct Awaitable;

template <class T = void> class Task {
public:
  using promise_type = Promise<T>;

  using handle_type = std::coroutine_handle<promise_type>;

  Task(handle_type handle) : handle_(handle) {}
  Task(Task &&other) : handle_(other.handle_) { other.handle_ = nullptr; }
  Task &operator=(Task &&other) {
    handle_.destroy();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;

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
    // if (handle_) {
    //   handle_.destroy();
    // }
  }

  handle_type handle_;
};

template <class T> struct Promise {
  using handle_type = std::coroutine_handle<Promise<T>>;

  T value_;
  std::coroutine_handle<> continuation_ = nullptr;

  auto initial_suspend() noexcept {
    std::cout << "initial suspend Promise<T>" << std::endl;
    // not lazy start immediately
    return std::suspend_never{};
  }

  auto final_suspend() noexcept {
    std::cout << "final suspend Promise<T>" << std::endl;
    // always suspend let destructor of task destroy coroutine
    struct awaitable {
      using promise_type = Promise<T>;
      using handle_type = std::coroutine_handle<Promise<T>>;

      std::coroutine_handle<> continuation_;

      bool await_ready() noexcept { return false; }

      auto await_suspend(std::coroutine_handle<> handle) noexcept {
        return continuation_;
      }

      void await_resume() noexcept {}

      awaitable(std::coroutine_handle<> cont) : continuation_(cont) {}
    } aw(continuation_);
    return aw;
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
    std::cout << "return_value: " << v << std::endl;
    set_value(std::forward<decltype(v)>(v));
  }
};

template <> struct Promise<void> {
  using handle_type = std::coroutine_handle<Promise<>>;

  std::coroutine_handle<> continuation_ = nullptr;

  auto initial_suspend() noexcept {
    std::cout << "initial suspend Promise<>" << std::endl;
    // not lazy start immediately
    return std::suspend_never{};
  }

  auto final_suspend() noexcept {
    std::cout << "final suspend Promise<>" << std::endl;
    // always suspend let destructor of task destroy coroutine
    return std::suspend_never{};
  }

  void unhandled_exception() {}

  Task<> get_return_object() {
    // return task to caller
    return handle_type::from_promise(*this);
  }

  Task<> get_task() { return get_return_object(); }

  void return_void() {
    if (continuation_) {
      continuation_.resume();
    }
  }
};

template <class T> auto operator co_await(Task<T> &&t) {
  std::cout << "co_await Task<T>" << std::endl;
  struct awaitable {
    using promise_type = Promise<T>;
    using handle_type = std::coroutine_handle<Promise<T>>;
    std::coroutine_handle<> handle_;
    bool await_ready() {
      std::cout << "await_ready" << std::endl;
      return false;
    }
    void await_suspend(std::coroutine_handle<Promise<>> handle) {
      std::cout << "f() suspend" << std::endl;
      std::cout << "handle: " << handle.address() << " done: " << handle.done()
                << std::endl;
      std::cout << "task handle: " << task_.handle_.address()
                << " done: " << task_.handle_.done() << std::endl;
      task_.handle_.promise().continuation_ = handle;
      // task_.handle_.resume();
    }
    T await_resume() {
      std::cout << "Task<> resumed" << std::endl;
      return task_.get_result().value();
    }
    Task<T> task_;
    awaitable(Task<T> &&t) : task_(std::move(t)) {}
  } aw(std::move(t));
  return aw;
}

auto this_coro() {
  struct {
    using handle_type = std::coroutine_handle<>;
    handle_type handle_;
    bool await_ready() { return false; }
    void await_suspend(handle_type handle) {
      std::cout << "this coro" << std::endl;
      handle_ = handle;
      handle.resume();
    }
    handle_type await_resume() { return handle_; }
  } aw;
  return aw;
}

} // namespace hm

using namespace hm;
std::coroutine_handle<> coro;
// inner task
Task<int> f1() {
  std::cout << "entering f1()" << std::endl;
  coro = co_await this_coro();
  std::cout << "suspending coro" << std::endl;
  co_await std::suspend_always{};
  std::cout << "f1() resumed" << std::endl;
  co_return 69;
}
// outer task
Task<> f() {
  std::cout << "entering f()" << std::endl;
  // co_await f1();
  int x = co_await f1();
  std::cout << "resumed f() " << std::endl;
  std::cout << "fuck" << std::endl;
  std::cout << x << std::endl;
}

int main() {
  auto t = f();
  std::cout << "in main" << std::endl;
  coro.resume();
}
