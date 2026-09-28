// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_POOL_GLOBAL_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POOL_GLOBAL_EXECUTOR_H

#include "base/core/debug.h"
#include "base/execution/executor/pool/bthread_executor.h"
#include "base/execution/executor/io/io_executor.h"
#include "base/execution/executor/pool/nthread_executor.h"
#include "base/memory/singleton.h"

namespace base {
namespace execution {

// Process-wide singleton executor; default thread count matches prior behavior.
template <typename ExecutorType, std::size_t kDefaultThreads = 16>
struct GlobalThreadPoolExecutor {
  using ThreadContext = typename ExecutorType::ThreadContext;
  template <typename Fn, typename... Args>
  inline auto execute(Fn&& fn, Args&&... args) {
    static auto* executor =
        base::Singleton<ExecutorType>::instance(kDefaultThreads);
    return executor->execute(std::forward<Fn>(fn), std::forward<Args>(args)...);
  }
};

using GlobalNThreadPoolExecutor = GlobalThreadPoolExecutor<NThreadPoolExecutor>;
using GlobalBThreadPoolExecutor = GlobalThreadPoolExecutor<BThreadPoolExecutor>;

struct GlobalIOExecutor {
  using ThreadContext = typename IOExecutor::ThreadContext;
  template <typename Fn, typename... Args>
  inline auto execute(Fn&& fn, Args&&... args) {
    static auto* executor = base::Singleton<IOExecutor>::instance();
    return executor->execute(std::forward<Fn>(fn), std::forward<Args>(args)...);
  }
};

}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_POOL_GLOBAL_EXECUTOR_H
