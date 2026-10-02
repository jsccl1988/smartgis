// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Startup-phase rollup for SmartGisViews: filters process_trace events with
// cat=="startup", prints a wall-time table, optional file dump.

#ifndef BASE_TRACE_DIAG_STARTUP_PROFILE_H_
#define BASE_TRACE_DIAG_STARTUP_PROFILE_H_

#include "base/core/export.h"

namespace base {
namespace trace {

// SMT_STARTUP_PROFILE=1 enables process tracing (same as SMT_TRACE=1) so
// BASE_TRACE_EVENT(..., "startup") spans are captured even without always-on
// diagnostics. Safe to call more than once; no-op when env unset.
BASE_EXPORT void maybe_init_startup_profile_from_env();

// True when SMT_STARTUP_PROFILE=1, or SMT_STARTUP_PROFILE_DUMP is set, or
// (Debug builds) always — used by the shell to dump once after first show.
BASE_EXPORT bool startup_profile_wanted();

// Print a sorted phase table (offset_ms / dur_ms / name) for cat=="startup"
// events to stderr + LOGGING. If path is non-null/non-empty, also write the
// same text (and chrome JSON of the full buffer) there. No-op when empty.
// Safe across DLL boundaries (snapshots inside base.dll).
BASE_EXPORT void dump_startup_profile(const char* path = nullptr);

// Mid-startup snapshot that does NOT claim the final dump slot and does NOT
// overwrite the canonical startup_profile.txt. Writes sibling
// startup_profile.partial-<tag>.txt (and .json) under the default dump dir,
// or <SMT_STARTUP_PROFILE_DUMP>.partial-<tag> when that env is set.
// |tag| should be a short ASCII token (e.g. "pre-attach").
BASE_EXPORT void dump_startup_profile_partial(const char* tag);

// Once per process: if startup_profile_wanted(), dump to
// SMT_STARTUP_PROFILE_DUMP (if set) else default Debug path / stderr.
BASE_EXPORT void maybe_dump_startup_profile();

}  // namespace trace
}  // namespace base

#endif  // BASE_TRACE_DIAG_STARTUP_PROFILE_H_
