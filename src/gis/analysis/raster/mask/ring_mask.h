// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_MASK_RING_MASK_H_
#define GIS_ANALYSIS_RASTER_MASK_RING_MASK_H_

#include <cstddef>
#include <cstdint>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// Axis-aligned skip box for one ring. |valid| false skips the ring.
struct RingMaskBBox {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  bool valid = false;
};

// Even-odd point-in-ring. |n| < 3 is outside. The ring may be open
// (last vertex need not repeat the first). Reads |x|/|y| in place.
inline bool point_in_ring(double px, double py, const double* x, const double* y,
                          size_t n) {
  if (!x || !y || n < 3) {
    return false;
  }
  bool inside = false;
  size_t j = n - 1;
  for (size_t i = 0; i < n; ++i) {
    const double xi = x[i];
    const double yi = y[i];
    const double xj = x[j];
    const double yj = y[j];
    const bool intersect =
        ((yi > py) != (yj > py)) &&
        (px < (xj - xi) * (py - yi) / ((yj - yi) + 0.0) + xi);
    if (intersect) {
      inside = !inside;
    }
    j = i;
  }
  return inside;
}

// Cell-center even-odd mask. Row 0 is north (|maxy|). |out| is |cols|*|rows|
// bytes, 1 = inside any ring. |ring_off| has |ring_count|+1 entries.
// |bboxes| may be null; when set, invalid boxes and misses skip that ring.
// Writes every cell. No CUDA.
GIS_EXPORT void fill_ring_mask(double minx, double miny, double maxx, double maxy,
                              int cols, int rows, const double* ring_x,
                              const double* ring_y, const int* ring_off,
                              int ring_count, const RingMaskBBox* bboxes,
                              uint8_t* out);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_MASK_RING_MASK_H_
