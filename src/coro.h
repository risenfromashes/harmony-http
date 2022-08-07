#pragma once

#include <coroutine>
#include <iostream>

namespace hm {

struct SuspendAndContinue {
  using coro = std::coroutine_handle<>;
  coro continuation;
  bool await_ready() noexcept { return false; }
  void await_suspend(coro handle) noexcept {
    if (continuation) {
      continuation.resume();
    }
  }
  void await_resume() noexcept {}
  SuspendAndContinue(coro cont) : continuation(cont) {}
};

inline auto this_coro() {
  struct {
    using handle_type = std::coroutine_handle<>;
    handle_type handle_;
    bool await_ready() { return false; }
    void await_suspend(handle_type handle) {
      handle_ = handle;
      handle.resume();
    }
    handle_type await_resume() { return handle_; }
  } aw;
  return aw;
}

} // namespace hm
