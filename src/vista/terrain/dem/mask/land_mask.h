// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_MASK_LAND_MASK_H_
#define VISTA_TERRAIN_DEM_MASK_LAND_MASK_H_

#include <cstdint>
#include <vector>

namespace vista {

// Closed lon/lat ring used to mask a rectangular DEM into a country outline.
// prepare_bbox() lets any_ring_contains skip full even-odd tests when the
// query point is outside a ring's axis-aligned envelope (china_city scale).
struct LonLatRing {
  std::vector<double> x;
  std::vector<double> y;
  mutable double minx = 0;
  mutable double miny = 0;
  mutable double maxx = 0;
  mutable double maxy = 0;
  mutable bool has_bbox = false;
  bool empty() const { return x.size() < 3 || x.size() != y.size(); }
  void prepare_bbox() const;
  bool bbox_may_contain(double px, double py) const;
};

// Even-odd point-in-polygon. Ring is closed or open (last≠first is OK).
bool point_in_lonlat_ring(double px, double py, const LonLatRing& ring);

bool any_ring_contains(double px, double py,
                       const std::vector<LonLatRing>& rings);

// Cell-center even-odd mask for a regular lon/lat grid. Row 0 is maxy.
// |out| must hold cols*rows bytes (1 = inside any ring). Tries Thrust GPU
// first; otherwise prepares ring bboxes and fills rows with parallel_for.
void fill_lonlat_mask(double minx, double miny, double maxx, double maxy,
                      int cols, int rows, const std::vector<LonLatRing>& rings,
                      uint8_t* out);

// Last fill_lonlat_mask clocks for equal-profile PIP bench.
struct LandMaskBakeSample {
  int64_t fill_ms = 0;
  int used_cuda = 0;
  int cols = 0;
  int rows = 0;
};

LandMaskBakeSample land_mask_last_bake_sample();
void reset_land_mask_bake_sample();

}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_MASK_LAND_MASK_H_
