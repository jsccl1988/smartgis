// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_POOL_NEW_THREAD_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POOL_NEW_THREAD_EXECUTOR_H

#include "base/core/debug.h"
#include "base/execution/executor/contexts/nthread.h"

namespace base {
namespace execution {
// like std::async
struct NewThreadExecutor
    : public ThreadContext<std::mutex, std::condition_variable, std::thread> {
  template <typename Fn, typename... Args>
  constexpr auto execute(Fn &&fn, Args &&...args) {
    auto task =
        make_twoway_task(std::forward<Fn>(fn), std::forward<Args>(args)...);
    auto future = task->get_future();
    std::thread([task]() {
      (*task)();
      delete task;
    }).detach();
    return future;
  }
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_POOL_NEW_THREAD_EXECUTOR_H