// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Always-on Diagnostic Tools bootstrap: tracing + AllocationTracker + memory
// counter sampling for Output / CPU / Memory panes.

#ifndef BASE_TRACE_DIAGNOSTIC_BOOTSTRAP_H_
#define BASE_TRACE_DIAGNOSTIC_BOOTSTRAP_H_

#include "base/core/export.h"

namespace base {

// Enable process-wide tracing (if off), AllocationTracker, and a 500ms memory
// counter sampler into process_trace(). Safe to call more than once.
BASE_EXPORT void start_always_on_diagnostics();

// Stop the memory sampler thread. Does not disable tracing / AllocationTracker
// (Stop in the Diagnostic Tools toolbar still owns those).
BASE_EXPORT void stop_always_on_diagnostics();

BASE_EXPORT bool always_on_diagnostics_started();

}  // namespace base

#endif  // BASE_TRACE_DIAGNOSTIC_BOOTSTRAP_H_
