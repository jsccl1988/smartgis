// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_POOL_INLINE_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POOL_INLINE_EXECUTOR_H

#include "base/core/debug.h"
#include "base/execution/executor/contexts/nthread.h"

namespace base {
namespace execution {
struct InlineExecutor {
  using ThreadContext = NThreadPool;
  template <typename Fn, typename... Args>
  constexpr auto execute(Fn &&fn, Args &&...args) {
    return fn(std::forward<Args>(args)...);
  }
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_POOL_INLINE_EXECUTOR_H