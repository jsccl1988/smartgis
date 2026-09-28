// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_POOL_THREAD_POOL_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POOL_THREAD_POOL_EXECUTOR_H

#include "base/core/debug.h"

namespace base {
namespace execution {

// Generic executor over a pool type that exposes emplace(), run(), join() and
// uses Pool::Thread for default sizing (same contract as NThreadPool / BThreadPool).
template <typename Pool>
class ThreadPoolExecutor {
 public:
  using ThreadContext = Pool;

  explicit ThreadPoolExecutor(
      std::size_t size = ThreadContext::Thread::hardware_concurrency())
      : _pool(size) {
    _pool.run();
  }

  ~ThreadPoolExecutor() { _pool.join(); }

  template <typename Fn, typename... Args>
  constexpr auto execute(Fn&& fn, Args&&... args) noexcept {
    return _pool.emplace(std::forward<Fn>(fn), std::forward<Args>(args)...);
  }

 private:
  Pool _pool;
  DISALLOW_COPY_AND_ASSIGN(ThreadPoolExecutor);
};

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTOR_POOL_THREAD_POOL_EXECUTOR_H
