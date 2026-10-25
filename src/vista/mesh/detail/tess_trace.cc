// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/mesh/detail/tess_trace.h"

#include "base/memory/arena.h"
#include "vista/mesh/tessellate.h"

namespace vista {
namespace detail {
namespace {

// Per-worker pending us; flushed before arena clear / process_trace publish.
struct TessTlsBuckets {
  int64_t geom_us = 0;
  int64_t poly_fan_us = 0;
  int64_t line_us = 0;
  int64_t line_points_us = 0;
  int64_t line_dash_us = 0;
  int64_t line_solid_us = 0;
};

TessTlsBuckets& tess_tls() {
  thread_local TessTlsBuckets b;
  return b;
}

int64_t* tls_slot_for(std::atomic<int64_t>* bucket) {
  TessTraceStats& s = tess_trace_stats();
  TessTlsBuckets& t = tess_tls();
  if (bucket == &s.geom_us) {
    return &t.geom_us;
  }
  if (bucket == &s.poly_fan_us) {
    return &t.poly_fan_us;
  }
  if (bucket == &s.line_us) {
    return &t.line_us;
  }
  if (bucket == &s.line_points_us) {
    return &t.line_points_us;
  }
  if (bucket == &s.line_dash_us) {
    return &t.line_dash_us;
  }
  if (bucket == &s.line_solid_us) {
    return &t.line_solid_us;
  }
  return nullptr;
}

}  // namespace

TessTraceStats& tess_trace_stats() {
  static TessTraceStats stats;
  return stats;
}

void flush_tess_trace_tls() {
  TessTlsBuckets& t = tess_tls();
  TessTraceStats& s = tess_trace_stats();
  auto push = [](std::atomic<int64_t>& dst, int64_t* src) {
    if (*src <= 0) {
      return;
    }
    dst.fetch_add(*src, std::memory_order_relaxed);
    *src = 0;
  };
  push(s.geom_us, &t.geom_us);
  push(s.poly_fan_us, &t.poly_fan_us);
  push(s.line_us, &t.line_us);
  push(s.line_points_us, &t.line_points_us);
  push(s.line_dash_us, &t.line_dash_us);
  push(s.line_solid_us, &t.line_solid_us);
}

ScopedTessCpu::ScopedTessCpu(std::atomic<int64_t>* b) {
  if (!base::trace::tracing_enabled() || !b || !tls_slot_for(b)) {
    return;
  }
  bucket = b;
  begin = std::chrono::steady_clock::now();
}

ScopedTessCpu::~ScopedTessCpu() {
  if (!bucket) {
    return;
  }
  int64_t* slot = tls_slot_for(bucket);
  if (!slot) {
    return;
  }
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now() - begin)
                      .count();
  *slot += us;
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
  flush_tess_trace_tls();
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(64 * 1024);
  }
}

}  // namespace detail

void reset_tess_trace_stats() {
  detail::flush_tess_trace_tls();
  detail::tess_trace_stats().reset();
}

void flush_tess_trace_stats() {
  if (!base::trace::tracing_enabled()) {
    return;
  }
  detail::flush_tess_trace_tls();
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
