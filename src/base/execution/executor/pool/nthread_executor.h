// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_POOL_NTHREAD_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POOL_NTHREAD_EXECUTOR_H

#include "base/execution/executor/contexts/nthread.h"
#include "base/execution/executor/pool/thread_pool_executor.h"

namespace base {
namespace execution {

using NThreadPoolExecutor = ThreadPoolExecutor<NThreadPool>;

}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_POOL_NTHREAD_EXECUTOR_H
