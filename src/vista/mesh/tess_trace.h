// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_TERRAIN_MESH_TESS_TRACE_H_
#define GIS_VISTA_WORLD_TERRAIN_MESH_TESS_TRACE_H_

#include <atomic>
#include <chrono>
#include <cstdint>

#include "base/trace/event/process_trace.h"

namespace vista {
namespace detail {

// Process-trace CPU buckets for tessellate_* under parallel_for.
struct TessTraceStats {
  std::atomic<int64_t> geom_us{0};
  std::atomic<int64_t> geom_n{0};
  std::atomic<int64_t> poly_fan_us{0};
  std::atomic<int64_t> line_us{0};
  std::atomic<int64_t> line_n{0};
  std::atomic<int64_t> line_points_us{0};
  std::atomic<int64_t> line_dash_us{0};
  std::atomic<int64_t> line_solid_us{0};

  void reset() {
    geom_us.store(0, std::memory_order_relaxed);
    geom_n.store(0, std::memory_order_relaxed);
    poly_fan_us.store(0, std::memory_order_relaxed);
    line_us.store(0, std::memory_order_relaxed);
    line_n.store(0, std::memory_order_relaxed);
    line_points_us.store(0, std::memory_order_relaxed);
    line_dash_us.store(0, std::memory_order_relaxed);
    line_solid_us.store(0, std::memory_order_relaxed);
  }
};

TessTraceStats& tess_trace_stats();

// RAII adder into one atomic CPU-us bucket when tracing is on.
struct ScopedTessCpu {
  std::atomic<int64_t>* bucket = nullptr;
  std::chrono::steady_clock::time_point begin{};

  explicit ScopedTessCpu(std::atomic<int64_t>* b) {
    if (!base::trace::tracing_enabled() || !b) {
      return;
    }
    bucket = b;
    begin = std::chrono::steady_clock::now();
  }

  ~ScopedTessCpu() {
    if (!bucket) {
      return;
    }
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - begin)
                        .count();
    bucket->fetch_add(us, std::memory_order_relaxed);
  }

  ScopedTessCpu(const ScopedTessCpu&) = delete;
  ScopedTessCpu& operator=(const ScopedTessCpu&) = delete;
};

void flush_tess_bucket(const char* name, int64_t us);

// Drop TLS arena scratch between tessellate_* entry points.
void clear_tessellate_tls_scratch();

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_WORLD_TERRAIN_MESH_TESS_TRACE_H_
