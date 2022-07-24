#include <concepts>
#include <coroutine>
#include <optional>

// basic coroutine resumable task
namespace hm {
template <class T = void> class Task {
public:
  struct promise_type;
  struct awaitable;

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

  auto operator co_await() { return awaitable(); }

private:
  handle_type handle_;
};

template <class T> struct Task<T>::promise_type {
  T value_;

  auto initial_suspend() {
    // not lazy start immediately
    return std::suspend_never{};
  }

  auto final_suspend() {
    // always suspend let destructor of task destroy coroutine
    return std::suspend_always{};
  }

  void unhandled_exception() {}

  Task<T> get_return_object() {
    // return task to caller
    return handle_type::from_promise(*this);
  }

  void return_value(std::convertible_to<T> auto &&v) {
    value_ = std::forward<decltype(v)>(v);
  }
};

template <> struct Task<void>::promise_type {
  auto initial_suspend() {
    // not lazy start immediately
    return std::suspend_never{};
  }

  auto final_suspend() {
    // always suspend let destructor of task destroy coroutine
    return std::suspend_always{};
  }

  void unhandled_exception() {}

  Task<> get_return_object() {
    // return task to caller
    return handle_type::from_promise(*this);
  }

  void return_void() {}
};

template <class T> struct Task<T>::awaitable {
  promise_type *promise_;

  bool await_ready() { return false; }

  void await_suspend(handle_type handle) { promise_ = &handle.promise(); }

  T await_resume() { return promise_->value_; }
};

template <> struct Task<void>::awaitable {

  bool await_ready() { return false; }

  void await_suspend(handle_type handle) {}

  void await_resume() {}
};

} // namespace hm
