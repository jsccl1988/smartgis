// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_DEM_FLOOD_FILL_H_
#define GIS_ANALYSIS_RASTER_DEM_FLOOD_FILL_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// DEM inundation result (RichDEM-style connected fill under a water surface).
struct FloodFillResult {
  bool ok = false;
  int width = 0;
  int height = 0;
  double geotransform[6] = {};
  // 1 = flooded, 0 = dry (row-major, size width*height).
  std::vector<unsigned char> mask;
  // Optional per-frame masks when frames > 1 (same size each).
  std::vector<std::vector<unsigned char>> frame_masks;
  double water_level = 0.0;
  std::string error;
};

// Pixel seed for 4-connected DEM fill (shared by flood_fill / storm_surge).
struct DemFloodSeed {
  int col = 0;
  int row = 0;
};

// 4-connected fill: cells with elev <= water_level reachable from any valid seed.
// Seeds out of bounds or above water are skipped. mask_out is resized to width*height.
GIS_EXPORT size_t flood_connected_at_level(const std::vector<float>& elev,
                                           int width,
                                           int height,
                                           const std::vector<DemFloodSeed>& seeds,
                                           double water_level,
                                           std::vector<unsigned char>* mask_out);

// Absolute water_level (map Z). Cells connected to seed with elev <= level flood.
GIS_EXPORT FloodFillResult run_flood_fill(std::string_view dem_path,
                                          double seed_x,
                                          double seed_y,
                                          double water_level,
                                          int frames);

// Write Byte GeoTIFF mask (1=flood). frames_dir optional for frame_*.tif.
GIS_EXPORT bool write_flood_mask_geotiff(std::string_view output_path,
                                         const FloodFillResult& result,
                                         std::string_view frames_dir);

// JSON: dem, output, seed_x, seed_y, water_level | water_depth,
// optional frames, frames_dir.
GIS_EXPORT bool run_flood_fill_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_DEM_FLOOD_FILL_H_
