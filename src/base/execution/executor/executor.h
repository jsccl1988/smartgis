// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Convenience dump for the executor track: L1 production surface plus optional
// backends. Prefer execution_executor.h when you want the documented L1 set
// only. IOUringExecutor and IoLane are not included here (liburing); include
// them explicitly. Storage SQEs go through IoLane, not IOUringExecutor.

#ifndef BASE_EXECUTION_EXECUTOR_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_EXECUTOR_H

#include "base/execution/execution_executor.h"
#include "base/execution/executor/policies/batch_executor.h"
#include "base/execution/executor/policies/lazy_executor.h"
#include "base/execution/executor/pool/numa_executor.h"
#include "base/execution/executor/pool/simd_executor.h"
#include "base/execution/executor/device/gpu_executor.h"

#endif  // BASE_EXECUTION_EXECUTOR_EXECUTOR_H
