// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_DEM_DEM_GRADIENT_H_
#define GIS_ANALYSIS_RASTER_DEM_DEM_GRADIENT_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// DEM finite-difference gradient (Eigen Map of the elevation grid).
struct DemGradientResult {
  bool ok = false;
  int width = 0;
  int height = 0;
  double geotransform[6] = {};
  // Row-major degrees; empty on failure.
  std::vector<float> slope_deg;
  std::vector<float> aspect_deg;
  std::string error;
};

// In-memory: elev row-major, cell sizes in map units (abs).
GIS_EXPORT DemGradientResult compute_dem_gradient(
    const std::vector<float>& elev,
    int width,
    int height,
    double cell_x,
    double cell_y);

// File DEM → slope/aspect GeoTIFF bands (Float32).
GIS_EXPORT DemGradientResult run_dem_gradient(std::string_view dem_path);

GIS_EXPORT bool write_dem_gradient_geotiff(std::string_view slope_path,
                                           std::string_view aspect_path,
                                           const DemGradientResult& result);

// JSON: dem, slope_output, aspect_output (aspect optional).
GIS_EXPORT bool run_dem_gradient_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_DEM_DEM_GRADIENT_H_
