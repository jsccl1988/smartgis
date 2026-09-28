// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_POOL_SIMD_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POOL_SIMD_EXECUTOR_H

#include "base/execution/executor/pool/nthread_executor.h"

namespace base {
namespace execution {

// Until SIMD-specific scheduling exists, use the same backing executor as
// NThreadPoolExecutor.
using SIMDExecutor = NThreadPoolExecutor;

}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_POOL_SIMD_EXECUTOR_H
