// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// io_uring executor is Linux+liburing only. This stub keeps include paths
// stable on Windows; do not instantiate.

#ifndef BASE_EXECUTION_EXECUTOR_IO_IO_URING_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_IO_IO_URING_EXECUTOR_H

#include "base/core/build_config.h"

#if defined(OS_LINUX) && defined(HAVE_LIBURING)
#error "HAVE_LIBURING io_uring_executor body not vendored in this tree yet"
#else

namespace base {
namespace execution {

struct IOUringExecutor {
  IOUringExecutor() = delete;
};

}  // namespace execution
}  // namespace base

#endif

#endif  // BASE_EXECUTION_EXECUTOR_IO_IO_URING_EXECUTOR_H
