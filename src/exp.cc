#include <concepts>
#include <coroutine>
#include <cstdio>
#include <iostream>
#include <optional>

// basic coroutine resumable task
#include "awaitabletask.h"
#include "coro.h"
#include "task.h"

using namespace hm;

std::coroutine_handle<> coro;
// inner task
AwaitableTask<int> f1() {
  std::cout << "entering f1()" << std::endl;
  coro = co_await this_coro();
  std::cout << "suspending coro" << std::endl;
  std::cout << "coro: " << coro.address() << std::endl;
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
  std::cout << "in main: coro: " << coro.address() << " done: " << coro.done()
            << " alive: " << bool(coro) << std::endl;
  coro.resume();
}
