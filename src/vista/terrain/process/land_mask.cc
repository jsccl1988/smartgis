// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/process/land_mask.h"

#include "gis/analysis/raster/mask/ring_mask.h"
#include "vista/terrain/process/nv/thrust_gis.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "vista/terrain/process/bake_backend.h"

namespace {

std::atomic<int64_t> g_fill_ms{0};
std::atomic<int> g_fill_cuda{0};
std::atomic<int> g_fill_cols{0};
std::atomic<int> g_fill_rows{0};

}  // namespace

namespace vista {

void LonLatRing::prepare_bbox() const {
  has_bbox = false;
  if (empty()) {
    return;
  }
  minx = maxx = x[0];
  miny = maxy = y[0];
  for (size_t i = 1; i < x.size(); ++i) {
    minx = (std::min)(minx, x[i]);
    maxx = (std::max)(maxx, x[i]);
    miny = (std::min)(miny, y[i]);
    maxy = (std::max)(maxy, y[i]);
  }
  has_bbox = true;
}

bool LonLatRing::bbox_may_contain(double px, double py) const {
  if (!has_bbox) {
    prepare_bbox();
  }
  return has_bbox && px >= minx && px <= maxx && py >= miny && py <= maxy;
}

bool point_in_lonlat_ring(double px, double py, const LonLatRing& ring) {
  if (ring.empty()) {
    return false;
  }
  return gis::detail::point_in_ring(px, py, ring.x.data(), ring.y.data(),
                                   ring.x.size());
}

bool any_ring_contains(double px, double py,
                       const std::vector<LonLatRing>& rings) {
  for (const LonLatRing& ring : rings) {
    if (!ring.bbox_may_contain(px, py)) {
      continue;
    }
    if (point_in_lonlat_ring(px, py, ring)) {
      return true;
    }
  }
  return false;
}

void fill_lonlat_mask(double minx, double miny, double maxx, double maxy,
                      int cols, int rows, const std::vector<LonLatRing>& rings,
                      uint8_t* out) {
  if (!out || cols < 1 || rows < 1) {
    return;
  }
  const auto t0 = std::chrono::steady_clock::now();
  BASE_TRACE_EVENT("fill_lonlat_mask", "bake");
  g_fill_cuda.store(0, std::memory_order_relaxed);
  g_fill_cols.store(cols, std::memory_order_relaxed);
  g_fill_rows.store(rows, std::memory_order_relaxed);
  for (const LonLatRing& ring : rings) {
    ring.prepare_bbox();
  }
  std::vector<double> ring_x;
  std::vector<double> ring_y;
  std::vector<int> ring_off;
  ring_off.push_back(0);
  for (const LonLatRing& ring : rings) {
    if (ring.empty()) {
      ring_off.push_back(static_cast<int>(ring_x.size()));
      continue;
    }
    ring_x.insert(ring_x.end(), ring.x.begin(), ring.x.end());
    ring_y.insert(ring_y.end(), ring.y.begin(), ring.y.end());
    ring_off.push_back(static_cast<int>(ring_x.size()));
  }
  const BakeBackend backend = bake_backend_from_env();
  const bool allow_cuda = backend != BakeBackend::kCpu;
  const bool require_cuda = backend == BakeBackend::kCuda;
  if (allow_cuda &&
      try_fill_lonlat_mask_thrust(
          minx, miny, maxx, maxy, cols, rows, ring_x.data(), ring_y.data(),
          ring_off.data(), static_cast<int>(rings.size()),
          static_cast<int>(ring_x.size()), out)) {
    g_fill_cuda.store(1, std::memory_order_relaxed);
    g_fill_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count(),
                    std::memory_order_relaxed);
    return;
  }
  if (require_cuda) {
    g_fill_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count(),
                    std::memory_order_relaxed);
    return;
  }
  std::vector<gis::detail::RingMaskBBox> bboxes(rings.size());
  for (size_t i = 0; i < rings.size(); ++i) {
    bboxes[i].minx = rings[i].minx;
    bboxes[i].miny = rings[i].miny;
    bboxes[i].maxx = rings[i].maxx;
    bboxes[i].maxy = rings[i].maxy;
    bboxes[i].valid = rings[i].has_bbox && !rings[i].empty();
  }
  gis::detail::fill_ring_mask(minx, miny, maxx, maxy, cols, rows, ring_x.data(),
                             ring_y.data(), ring_off.data(),
                             static_cast<int>(rings.size()), bboxes.data(), out);
  g_fill_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0)
                      .count(),
                  std::memory_order_relaxed);
}

LandMaskBakeSample land_mask_last_bake_sample() {
  LandMaskBakeSample s;
  s.fill_ms = g_fill_ms.load(std::memory_order_relaxed);
  s.used_cuda = g_fill_cuda.load(std::memory_order_relaxed);
  s.cols = g_fill_cols.load(std::memory_order_relaxed);
  s.rows = g_fill_rows.load(std::memory_order_relaxed);
  return s;
}

void reset_land_mask_bake_sample() {
  g_fill_ms.store(0, std::memory_order_relaxed);
  g_fill_cuda.store(0, std::memory_order_relaxed);
  g_fill_cols.store(0, std::memory_order_relaxed);
  g_fill_rows.store(0, std::memory_order_relaxed);
}

}  // namespace vista
