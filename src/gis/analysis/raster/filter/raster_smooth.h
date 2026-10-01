// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_FILTER_RASTER_SMOOTH_H_
#define GIS_ANALYSIS_RASTER_FILTER_RASTER_SMOOTH_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

struct RasterSmoothResult {
  bool ok = false;
  int width = 0;
  int height = 0;
  double geotransform[6] = {};
  std::vector<float> values;
  std::string error;
};

// Discrete 5-point Laplace on unknown cells (is_unknown != 0). Dirichlet cells
// keep their values. Solves with Eigen SparseLU (scalar field).
GIS_EXPORT RasterSmoothResult smooth_raster_laplace(
    const std::vector<float>& values,
    int width,
    int height,
    const std::uint8_t* is_unknown);

GIS_EXPORT bool run_raster_smooth_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_FILTER_RASTER_SMOOTH_H_
