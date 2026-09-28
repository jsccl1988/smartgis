// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// L1 umbrella: production executor track (CPU pools, inline, new-thread, IO
// event-loop, process-wide globals, plus ThreadContext).
// Does not pull async, futures, interop, parallel, pipeline, or map_reduce.
// Optional backends (GPU, io_uring, IoLane, NUMA, SIMD alias) and decorator
// policies (batch, lazy under executor/policies/) stay explicit.
// See docs/superpowers/specs/2026-09-06-base-inc-alignment-design.md.

#ifndef BASE_EXECUTION_EXECUTION_EXECUTOR_H
#define BASE_EXECUTION_EXECUTION_EXECUTOR_H

#include "base/execution/executor/contexts/context.h"
#include "base/execution/executor/pool/thread_pool_executor.h"
#include "base/execution/executor/pool/nthread_executor.h"
#include "base/execution/executor/pool/bthread_executor.h"
#include "base/execution/executor/pool/inline_executor.h"
#include "base/execution/executor/pool/new_thread_executor.h"
#include "base/execution/executor/io/io_executor.h"
#include "base/execution/executor/pool/global_executor.h"

#endif  // BASE_EXECUTION_EXECUTION_EXECUTOR_H
