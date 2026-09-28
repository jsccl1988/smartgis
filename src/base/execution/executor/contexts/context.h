// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_CONTEXTS_H
#define BASE_EXECUTION_EXECUTOR_CONTEXTS_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

#include "base/core/log.h"
#include "base/core/macros.h"
#include "base/synchronization/align.h"
#include "base/synchronization/future.h"

namespace base {
namespace execution {
struct Task {
  virtual ~Task() {}
  inline virtual void operator()() = 0;
};

template <typename T, typename M, typename C>
class TaskQueue {
 public:
  using Mutex = M;
  using ConditionVariable = C;
  using value_type = T;

  template <typename U>
  bool push(U&& obj) {
    if (UNLIKELY(_close)) {
      return false;
    }

    {
      std::lock_guard<Mutex> lock{_lock};
      _queue.push(std::forward<U>(obj));
    }

    _cv.notify_one();
    return true;
  }

  bool pop(T& obj) {
    {
      std::unique_lock<Mutex> lock{_lock};
      _cv.wait(lock, [this]() -> bool { return _close || !_queue.empty(); });

      if (_close && _queue.empty()) {
        return false;
      }

      obj = std::move(_queue.front());
      _queue.pop();
    }

    return true;
  }

  void close() {
    _close = true;
    _cv.notify_all();
  }

  std::size_t size_approx() const { return _queue.size(); }
  std::size_t is_close() const { return _close; }

 private:
  std::queue<T> _queue;
  alignas(hardware_destructive_interference_size) std::atomic<bool> _close{
      false};
  Mutex _lock;
  ConditionVariable _cv;
};

// ADL rules:https://en.cppreference.com/w/cpp/language/adl
// Arguments of class type (including union), the set consists of
// a) The class itself
// b) All of its direct and indirect base classes
// c) If the class is a member of another class, the class of which it is a
// member
// d) The innermost enclosing namespaces of the classes added to the set
// so we can specify mutex/condition_variable... by inherit from ThreadContext
// example:
// using NativeThreadContext =
//     ThreadContext<std::mutex, std::condition_variable, std::thread>;
// class ThreadPool : public NativeThreadContext {
//   Mutex...
//   ConditionVariable...
//   Thread...
// };
template <typename M, typename C, typename T>
struct ThreadContext {
  using Mutex = M;
  using ConditionVariable = C;
  using Thread = T;

  template <typename U>
  using Future = future<U, Mutex, ConditionVariable>;

  template <typename U>
  using SharedFuture = shared_future<U, Mutex, ConditionVariable>;

  template <typename U>
  using Promise = promise<U, Mutex, ConditionVariable>;

  template <typename>
  class Function;
  template <class R, class... Args>
  class Function<R(Args...)> {
    Promise<R> promise;
    std::function<R(Args...)> fn;

   public:
    Function() noexcept {}

    template <typename... Ts>
    explicit Function(Ts&&... ts) : fn(std::forward<Ts>(ts)...) {}

    Function(Function&) = delete;
    Function& operator=(Function&) = delete;

    Function(Function&& rhs) noexcept { swap(rhs); }
    Function& operator=(Function&& __other) {
      Function(std::move(__other)).swap(*this);
      return *this;
    }

    template <typename... Ts>
    void operator()(Ts&&... ts) {
      if constexpr (std::is_void_v<R>) {
        fn(std::forward<Ts>(ts)...);
        promise.set_value();
      } else {
        promise.set_value(fn(std::forward<Ts>(ts)...));
      }
    }

    auto get_future() { return promise.get_future(); }

    void swap(Function& other) {
      promise.swap(other.promise);
      fn.swap(other.fn);
    }

    bool valid() const { return static_cast<bool>(fn); }
    void reset() { Function(std::move(fn)).swap(*this); }
  };

  template <typename R, typename... Args>
  struct PackagedTask : public Task {
    Function<R(Args...)> fn;
    std::tuple<Args...> args;

    template <typename Fn, typename... Ts>
    PackagedTask(Fn&& fn_, Ts&&... args_) noexcept
        : fn(std::forward<Fn>(fn_)), args(std::forward<Ts>(args_)...) {}

    auto get_future() { return fn.get_future(); }
    void operator()() override { std::apply(fn, std::move(args)); }

    DISALLOW_COPY_AND_ASSIGN(PackagedTask);
  };

  // Heap-allocate tasks with `new`; the owning pool must `delete` after run.
  // Do not use ObjectAllocator/`base::create` here — tasks outlive any arena batch.
  template <typename Fn, typename... Args>
  inline auto make_twoway_task(Fn&& fn, Args&&... args) noexcept {
    using R = typename std::invoke_result<Fn, Args...>::type;
    return new PackagedTask<R, Args...>(std::forward<Fn>(fn),
                                        std::forward<Args>(args)...);
  }
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_CONTEXTS_H
