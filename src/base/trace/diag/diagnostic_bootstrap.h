// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Always-on Diagnostic Tools bootstrap: tracing + AllocationTracker + memory
// counter sampling for Output / CPU / Memory panes.

#ifndef BASE_TRACE_DIAG_DIAGNOSTIC_BOOTSTRAP_H_
#define BASE_TRACE_DIAG_DIAGNOSTIC_BOOTSTRAP_H_

#include <cstdint>

#include "base/core/export.h"

namespace base {
namespace trace {

// OS counters for the current process. Arena `capacity()` is SIZE_MAX on the
// synchronized pool and is not a process limit.
struct ProcessMemorySnapshot {
  uint64_t working_set = 0;
  uint64_t private_bytes = 0;
  uint64_t peak_working_set = 0;
  uint64_t physical_total = 0;
};

BASE_EXPORT ProcessMemorySnapshot process_memory_snapshot();

// Enable process-wide tracing (if off), AllocationTracker, and a 500ms memory
// counter sampler into process_trace(). Safe to call more than once.
BASE_EXPORT void start_always_on_diagnostics();

// Stop the memory sampler thread. Does not disable tracing / AllocationTracker
// (Stop in the Diagnostic Tools toolbar still owns those).
BASE_EXPORT void stop_always_on_diagnostics();

BASE_EXPORT bool always_on_diagnostics_started();

}  // namespace trace
}  // namespace base

#endif  // BASE_TRACE_DIAG_DIAGNOSTIC_BOOTSTRAP_H_
