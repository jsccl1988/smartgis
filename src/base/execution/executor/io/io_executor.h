// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Portable IO executor: one worker thread + task queue. No epoll / base/io.
// Linux epoll watcher APIs are deferred; use execute() for posted work.

#ifndef BASE_EXECUTION_EXECUTOR_IO_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_IO_EXECUTOR_H

#include <utility>

#include "base/core/macros.h"
#include "base/execution/executor/pool/nthread_executor.h"

namespace base {
namespace execution {

// Dedicated serial worker for IO-style callbacks (mogu IOExecutor Hybrid).
class IOExecutor {
 public:
  using ThreadContext = NThreadPoolExecutor::ThreadContext;

  IOExecutor() : pool_(1) {}

  template <typename Fn, typename... Args>
  constexpr auto execute(Fn&& fn, Args&&... args) noexcept {
    return pool_.execute(std::forward<Fn>(fn), std::forward<Args>(args)...);
  }

 private:
  NThreadPoolExecutor pool_;
  DISALLOW_COPY_AND_ASSIGN(IOExecutor);
};

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTOR_IO_EXECUTOR_H
