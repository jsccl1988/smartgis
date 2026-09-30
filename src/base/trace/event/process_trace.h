// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Process-wide Trace for Views render profiling (always-on diagnostics,
// SMT_TRACE=1, or UI Record). Implementation lives in base.dll (one buffer
// across content / ui_views / exe).

#ifndef BASE_TRACE_EVENT_PROCESS_TRACE_H_
#define BASE_TRACE_EVENT_PROCESS_TRACE_H_

#include <atomic>
#include <string>
#include <string_view>

#include "base/core/export.h"
#include "base/trace/event/trace.h"

namespace base {
namespace trace {

BASE_EXPORT Trace& process_trace();
BASE_EXPORT std::atomic<bool>& tracing_enabled_flag();
BASE_EXPORT bool tracing_enabled();
BASE_EXPORT void set_tracing_enabled(bool on);

// Free-function span record for /GL clients. Calling Trace::add from an
// inline dtor in another module can LNK2005 against base.dll under LTCG.
BASE_EXPORT void process_trace_add(std::string_view name,
                                   std::string_view cat,
                                   Trace::time_point begin,
                                   Trace::time_point end);

// Call once at process start. SMT_TRACE=1 enables recording.
BASE_EXPORT void maybe_init_tracing_from_env();

// If SMT_TRACE_DUMP=<path> is set, write Chrome Trace JSON and print a rollup
// to stderr. Safe to call when tracing is empty / disabled.
BASE_EXPORT void maybe_dump_tracing_to_env();

// DLL-safe snapshot: walks events inside base.dll and reports POD + C strings.
// Callers must copy strings into their own module — do not assign
// process_trace().snapshot_events() across DLL boundaries (MSVC debug iterators).
using ProcessTraceEventFn = void (*)(void* ctx,
                                     int tid,
                                     Trace::time_point begin,
                                     Trace::time_point end,
                                     const char* name,
                                     const char* cat,
                                     Trace::Event::Kind kind,
                                     int64_t counter_value);
BASE_EXPORT void for_each_process_trace_event(ProcessTraceEventFn fn,
                                              void* ctx);
BASE_EXPORT Trace::time_point process_trace_origin();

// RAII span that no-ops when tracing is disabled (hot path cheap check).
struct ScopedTraceEvent {
  using time_point = Trace::time_point;

  bool active = false;
  std::string name;
  std::string cat;
  time_point begin{};

  ScopedTraceEvent(std::string_view name_, std::string_view cat_) {
    if (!tracing_enabled()) {
      return;
    }
    active = true;
    name = name_;
    cat = cat_;
    begin = time_point::clock::now();
  }

  ~ScopedTraceEvent() {
    if (!active) {
      return;
    }
    process_trace_add(name, cat, begin, time_point::clock::now());
  }

  ScopedTraceEvent(const ScopedTraceEvent&) = delete;
  ScopedTraceEvent& operator=(const ScopedTraceEvent&) = delete;
};

}  // namespace trace
}  // namespace base

#define BASE_TRACE_EVENT(name, cat) \
  ::base::trace::ScopedTraceEvent BASE_CONCAT_TRACE_(__LINE__)(name, cat)

#define BASE_CONCAT_TRACE_INNER_(a, b) a##b
#define BASE_CONCAT_TRACE_(line) BASE_CONCAT_TRACE_INNER_(_trace_ev_, line)

#endif  // BASE_TRACE_EVENT_PROCESS_TRACE_H_
