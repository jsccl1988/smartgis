// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_FILTER_RASTER_CONVOLVE_H_
#define GIS_ANALYSIS_RASTER_FILTER_RASTER_CONVOLVE_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

struct RasterConvolveResult {
  bool ok = false;
  int width = 0;
  int height = 0;
  double geotransform[6] = {};
  std::vector<float> values;
  std::string error;
};

// Separable? No — general kxk kernel (k odd). Border: replicate.
GIS_EXPORT RasterConvolveResult convolve_raster(
    const std::vector<float>& input,
    int width,
    int height,
    const std::vector<double>& kernel,
    int kernel_w,
    int kernel_h);

// Built-in 3x3 box mean.
GIS_EXPORT RasterConvolveResult convolve_box3(const std::vector<float>& input,
                                              int width,
                                              int height);

GIS_EXPORT bool run_raster_convolve_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_FILTER_RASTER_CONVOLVE_H_
