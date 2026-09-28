// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// SmartGIS: brpc BThread is not vendored. BThreadPool aliases NThreadPool so
// L1 BThreadPoolExecutor stays available with std::thread workers.

#ifndef BASE_EXECUTION_BTHREAD_CONTEXT_H
#define BASE_EXECUTION_BTHREAD_CONTEXT_H

#include "base/execution/executor/contexts/nthread.h"

namespace base {
namespace execution {

using BThreadPool = NThreadPool;

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_BTHREAD_CONTEXT_H
