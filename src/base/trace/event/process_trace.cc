// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Process-wide Trace singleton lives in base.dll so map2d/scene3d (content),
// RenderTracePanel (ui_views), and SMT_TRACE_DUMP (exe) share one buffer.

#include "base/trace/event/process_trace.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace base {
namespace trace {

Trace& process_trace() {
  static Trace g_trace;
  return g_trace;
}

std::atomic<bool>& tracing_enabled_flag() {
  static std::atomic<bool> g_enabled{false};
  return g_enabled;
}

bool tracing_enabled() {
  return tracing_enabled_flag().load(std::memory_order_relaxed);
}

void set_tracing_enabled(bool on) {
  // Clear only on off → on so always-on diagnostics keep startup spans when
  // the UI re-arms while already recording. Explicit Record still clears via
  // process_trace().clear() before enable.
  const bool was =
      tracing_enabled_flag().exchange(on, std::memory_order_relaxed);
  if (on && !was) {
    process_trace().clear();
  }
}

void process_trace_add(std::string_view name,
                       std::string_view cat,
                       Trace::time_point begin,
                       Trace::time_point end) {
  process_trace().add(name, cat, begin, end);
}

void maybe_init_tracing_from_env() {
  if (const char* env = std::getenv("SMT_TRACE")) {
    if (env[0] == '1' && env[1] == '\0') {
      set_tracing_enabled(true);
    }
  }
  // SMT_STARTUP_PROFILE=1 also arms recording so cat=startup spans land even
  // when always-on diagnostics are skipped (e.g. utility/gpu helpers).
  if (const char* env = std::getenv("SMT_STARTUP_PROFILE")) {
    if (env[0] == '1' && env[1] == '\0') {
      set_tracing_enabled(true);
    }
  }
}

void maybe_dump_tracing_to_env() {
  const char* path = std::getenv("SMT_TRACE_DUMP");
  if (!path || !path[0]) {
    return;
  }
  const auto events = process_trace().snapshot_events();
  const auto phases = rollup_trace_phases(events);
  {
    std::ofstream out(path, std::ios::binary);
    if (out) {
      out << process_trace().dump();
    }
  }
  std::fprintf(stderr,
               "[trace] dump=%s events=%zu phases=%zu\n", path, events.size(),
               phases.size());
  size_t n2 = 0;
  size_t n3 = 0;
  for (const auto& e : events) {
    if (e.cat.find("map2d") == 0 || e.name.find("map2d") == 0) {
      ++n2;
    }
    if (e.cat.find("scene3d") == 0 || e.name.find("scene3d") == 0) {
      ++n3;
    }
  }
  std::fprintf(stderr, "[trace] map2d_events=%zu scene3d_events=%zu\n", n2, n3);
  for (const auto& p : phases) {
    std::fprintf(stderr,
                 "[trace] phase name=%s n=%llu avg_us=%.1f p99_us=%llu "
                 "min_us=%llu max_us=%llu\n",
                 p.name.c_str(),
                 static_cast<unsigned long long>(p.count), p.avg_us,
                 static_cast<unsigned long long>(p.p99_us),
                 static_cast<unsigned long long>(p.min_us),
                 static_cast<unsigned long long>(p.max_us));
  }
}

void for_each_process_trace_event(ProcessTraceEventFn fn, void* ctx) {
  if (!fn) {
    return;
  }
  const auto events = process_trace().snapshot_events();
  for (const auto& e : events) {
    fn(ctx, e.tid, e.begin, e.end, e.name.c_str(), e.cat.c_str(), e.kind,
       e.counter_value);
  }
}

Trace::time_point process_trace_origin() {
  return process_trace().origin();
}

}  // namespace trace
}  // namespace base
