// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_SAMPLE_TRACE_H_
#define BASE_MEMORY_SAMPLE_TRACE_H_

#include "base/memory/allocation_tracker.h"
#include "base/memory/arena.h"
#include "base/trace/process_trace.h"

namespace base {

// Push Chrome Trace counter samples (ph "C") for Diagnostic Tools Memory tab.
inline void sample_memory_counters_to_process_trace() {
  if (!tracing_enabled()) {
    return;
  }
  if (MemoryResource* mr = memory_resource()) {
    process_trace().add_counter("process_used", "memory",
                                static_cast<int64_t>(mr->used()));
    process_trace().add_counter("process_capacity", "memory",
                                static_cast<int64_t>(mr->capacity()));
  }
  if (MemoryResource* tls = tls_memory_resource()) {
    process_trace().add_counter("tls_used", "memory",
                                static_cast<int64_t>(tls->used()));
  }
  if (AllocationTracker::is_enabled()) {
    process_trace().add_counter("tracker_live", "memory",
                                static_cast<int64_t>(AllocationTracker::live_bytes()));
    process_trace().add_counter(
        "tracker_peak", "memory",
        static_cast<int64_t>(AllocationTracker::peak_bytes()));
  }
}

}  // namespace base

#endif  // BASE_MEMORY_SAMPLE_TRACE_H_
