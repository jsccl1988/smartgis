// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/mesh/tess_trace.h"

#include "base/memory/arena.h"
#include "vista/mesh/tessellate.h"

namespace vista {
namespace detail {

TessTraceStats& tess_trace_stats() {
  static TessTraceStats stats;
  return stats;
}

void flush_tess_bucket(const char* name, int64_t us) {
  if (us <= 0) {
    return;
  }
  const auto end = std::chrono::steady_clock::now();
  const auto begin = end - std::chrono::microseconds(us);
  base::trace::process_trace().add(name, "map2d.tess", begin, end);
}

void clear_tessellate_tls_scratch() {
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(64 * 1024);
  }
}

}  // namespace detail

void reset_tess_trace_stats() {
  detail::tess_trace_stats().reset();
}

void flush_tess_trace_stats() {
  if (!base::trace::tracing_enabled()) {
    return;
  }
  detail::TessTraceStats& s = detail::tess_trace_stats();
  // One Complete span per bucket; duration is CPU-us sum (may exceed wall
  // under parallel_for — that is intentional for cost attribution).
  detail::flush_tess_bucket("tess_geom",
                            s.geom_us.load(std::memory_order_relaxed));
  detail::flush_tess_bucket("tess_poly_fan",
                            s.poly_fan_us.load(std::memory_order_relaxed));
  detail::flush_tess_bucket("tess_line",
                            s.line_us.load(std::memory_order_relaxed));
  detail::flush_tess_bucket("tess_line_points",
                            s.line_points_us.load(std::memory_order_relaxed));
  detail::flush_tess_bucket("tess_line_dash",
                            s.line_dash_us.load(std::memory_order_relaxed));
  detail::flush_tess_bucket("tess_line_solid",
                            s.line_solid_us.load(std::memory_order_relaxed));
  base::trace::process_trace().add_counter(
      "tess_geom_n", "map2d.tess",
      s.geom_n.load(std::memory_order_relaxed));
  base::trace::process_trace().add_counter(
      "tess_line_n", "map2d.tess",
      s.line_n.load(std::memory_order_relaxed));
}

}  // namespace vista
