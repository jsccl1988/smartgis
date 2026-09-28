// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_POOL_BTHREAD_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POOL_BTHREAD_EXECUTOR_H

#include "base/execution/executor/contexts/bthread.h"
#include "base/execution/executor/pool/thread_pool_executor.h"

namespace base {
namespace execution {

using BThreadPoolExecutor = ThreadPoolExecutor<BThreadPool>;

}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_POOL_BTHREAD_EXECUTOR_H
