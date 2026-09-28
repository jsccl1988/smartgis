// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Decorator-style executor: queue via emplace(), drain via run().
// Not part of the L1 production surface.

#ifndef BASE_EXECUTION_EXECUTOR_POLICIES_LAZY_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POLICIES_LAZY_EXECUTOR_H

#include "base/core/debug.h"
#include "base/execution/executor/contexts/nthread.h"
#include "base/memory/singleton.h"
#include "base/tuple/tuple.h"

namespace base {
namespace execution {
class LazyExecutor {
 public:
  using ThreadContext = NThreadPool;
  LazyExecutor(std::size_t size = ThreadContext::Thread::hardware_concurrency())
      : _thread_pool(size) {}
  ~LazyExecutor() { _thread_pool.join(); }

  template <typename Fn, typename... Args>
  constexpr auto emplace(Fn &&fn, Args &&...args) noexcept {
    return _thread_pool.emplace(std::forward<Fn>(fn),
                                std::forward<Args>(args)...);
  }

  auto run() noexcept { _thread_pool.run(); }

  template <typename Fn, typename... Args>
  constexpr auto execute(Fn &&fn, Args &&...args) noexcept {
    return _thread_pool.emplace(std::forward<Fn>(fn),
                                std::forward<Args>(args)...);
  }

 private:
  NThreadPool _thread_pool;
  DISALLOW_COPY_AND_ASSIGN(LazyExecutor);
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_POLICIES_LAZY_EXECUTOR_H
