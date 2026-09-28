// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// IoLane is Linux+liburing only. Stub on other platforms.

#ifndef BASE_EXECUTION_EXECUTOR_IO_IO_LANE_H
#define BASE_EXECUTION_EXECUTOR_IO_IO_LANE_H

#include "base/core/build_config.h"

#if defined(OS_LINUX) && defined(HAVE_LIBURING)
#error "HAVE_LIBURING IoLane body not vendored in this tree yet"
#else

namespace base {
namespace execution {

struct IoLane {
  IoLane() = delete;
};

}  // namespace execution
}  // namespace base

#endif

#endif  // BASE_EXECUTION_EXECUTOR_IO_IO_LANE_H
