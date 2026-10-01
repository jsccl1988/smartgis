// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_DEM_STORM_SURGE_STATS_H_
#define GIS_ANALYSIS_RASTER_DEM_STORM_SURGE_STATS_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// One depth-class bin (half-open [lo, hi); hi=+inf when unbounded).
struct StormSurgeDepthClass {
  double lo = 0;
  double hi = 0;  // +inf → use a large sentinel in JSON ("inf")
  bool unbounded_hi = false;
  int cell_count = 0;
  double area = 0;
};

// Buffer / overlap summary vs an optional impact vector layer.
struct StormSurgeOverlapStats {
  bool computed = false;
  double buffer_distance = 0;
  double inundation_area = 0;
  double buffer_area = 0;
  int impact_feature_count = 0;
  int intersect_feature_count = 0;
  double intersect_area = 0;
  std::string error;
};

// Disaster metrics: inundation area, depth classes, optional overlap.
struct StormSurgeStatsResult {
  bool ok = false;
  int width = 0;
  int height = 0;
  int wet_cells = 0;
  double cell_area = 0;
  double inundation_area = 0;
  std::vector<StormSurgeDepthClass> depth_classes;
  // Optional class codes (0=dry, 1..N = class index); empty if not requested.
  std::vector<unsigned char> class_mask;
  StormSurgeOverlapStats overlap;
  std::string error;
};

// Pixel map-space area from GDAL geotransform (|det| of affine 2x2).
GIS_EXPORT double storm_surge_cell_area(const double* geotransform);

// Count wet cells and map-space inundation area (mask: 1=wet).
GIS_EXPORT bool compute_inundation_area(const unsigned char* mask,
                                        int width,
                                        int height,
                                        const double* geotransform,
                                        int* wet_cells,
                                        double* area);

// Default breaks 0.5 / 1.0 / 2.0 → classes [0,0.5) [0.5,1) [1,2) [2,+inf).
// |depth_breaks| empty uses defaults. Depths on dry cells ignored.
GIS_EXPORT bool classify_storm_surge_depth(
    const float* depth,
    const unsigned char* mask,
    int width,
    int height,
    const double* geotransform,
    const std::vector<double>& depth_breaks,
    std::vector<StormSurgeDepthClass>* classes,
    std::vector<unsigned char>* class_mask_out);

// In-memory stats (mask required; depth optional; impact path optional).
GIS_EXPORT StormSurgeStatsResult compute_storm_surge_stats(
    const unsigned char* mask,
    const float* depth,
    int width,
    int height,
    const double* geotransform,
    const std::vector<double>& depth_breaks,
    std::string_view impact_path,
    double buffer_distance,
    bool build_class_mask);

// JSON op: mask (required), optional depth, impact, buffer_distance,
// depth_breaks[], output (stats JSON), class_mask_output, buffer_output,
// overlap_output.
GIS_EXPORT bool run_storm_surge_stats_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_DEM_STORM_SURGE_STATS_H_
