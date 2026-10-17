// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MEMORY_SAMPLE_TRACE_H_
#define BASE_MEMORY_SAMPLE_TRACE_H_

#include "base/memory/allocation_tracker.h"
#include "base/memory/arena.h"
#include "base/trace/diag/diagnostic_bootstrap.h"
#include "base/trace/event/process_trace.h"

namespace base {

// Push Chrome Trace counter samples (ph "C") for Diagnostic Tools Memory tab.
// |include_tls| constructs the calling thread's HybridOptimized TLS arena on
// first use — never pass true from a background sampler (debug CRT heap
// races STATUS_HEAP_CORRUPTION during PluginShell / CommandCatalog).
inline void sample_memory_counters_to_process_trace(bool include_tls = true) {
  if (!trace::tracing_enabled()) {
    return;
  }
  // Working set / physical RAM — not the PMR pool. SynchronizedPool::capacity
  // is SIZE_MAX and its used counter stays 0 when the process heap is CRT.
  const trace::ProcessMemorySnapshot os = trace::process_memory_snapshot();
  trace::process_trace().add_counter("process_used", "memory",
                                     static_cast<int64_t>(os.working_set));
  trace::process_trace().add_counter(
      "process_capacity", "memory", static_cast<int64_t>(os.physical_total));
  trace::process_trace().add_counter("process_private", "memory",
                                     static_cast<int64_t>(os.private_bytes));
  if (MemoryResource* mr = memory_resource()) {
    trace::process_trace().add_counter("arena_used", "memory",
                                       static_cast<int64_t>(mr->used()));
  }
  if (include_tls) {
    if (MemoryResource* tls = tls_memory_resource()) {
      trace::process_trace().add_counter("tls_used", "memory",
                                         static_cast<int64_t>(tls->used()));
    }
  }
  if (AllocationTracker::is_enabled()) {
    trace::process_trace().add_counter(
        "tracker_live", "memory",
        static_cast<int64_t>(AllocationTracker::live_bytes()));
    trace::process_trace().add_counter(
        "tracker_peak", "memory",
        static_cast<int64_t>(AllocationTracker::peak_bytes()));
  }
}

}  // namespace base

#endif  // BASE_MEMORY_SAMPLE_TRACE_H_
