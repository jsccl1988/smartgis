// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_DEM_STORM_SURGE_H_
#define GIS_ANALYSIS_RASTER_DEM_STORM_SURGE_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// Ocean-connected storm-surge inundation (DEM + coast/ocean seeds + surge levels).
struct StormSurgeResult {
  bool ok = false;
  int width = 0;
  int height = 0;
  double geotransform[6] = {};
  // 1 = inundated, 0 = dry (row-major).
  std::vector<unsigned char> mask;
  // Optional per-frame masks (same size each).
  std::vector<std::vector<unsigned char>> frame_masks;
  // Depth = max(0, surge_level - elev) on inundated cells; 0 elsewhere.
  std::vector<float> depth;
  std::vector<std::vector<float>> frame_depths;
  // Absolute water-surface Z used per frame.
  std::vector<double> surge_levels;
  std::string error;
};

// Seed from coast vector vertices and/or explicit map-space seed_xy (x,y pairs).
// surge_levels: absolute Z; if size==1 and frames>1, animate seed_min_z → level.
// Ocean-side connectivity: multi-seed flood_connected_at_level from coast/ocean.
GIS_EXPORT StormSurgeResult run_storm_surge(
    std::string_view dem_path,
    std::string_view coast_path,
    const std::vector<double>& seed_xy,
    const std::vector<double>& surge_levels,
    int frames);

// Write Byte GeoTIFF mask (1=wet). frames_dir optional for frame_*.tif.
GIS_EXPORT bool write_storm_surge_mask_geotiff(std::string_view output_path,
                                               const StormSurgeResult& result,
                                               std::string_view frames_dir);

// Write Float32 depth GeoTIFF (0=dry). frames_dir optional for depth_frame_*.tif.
GIS_EXPORT bool write_storm_surge_depth_geotiff(std::string_view output_path,
                                                const StormSurgeResult& result,
                                                std::string_view frames_dir);

// Polygonize wet mask (value==1) to GeoJSON MultiPolygon / polygons.
GIS_EXPORT bool polygonize_storm_surge_mask(std::string_view output_path,
                                            const StormSurgeResult& result);

// Water-surface triangle mesh over wet cells (shared-vertex grid).
// Z is the free-surface level (water_level). When |depth| is non-null and
// sized width*height, Z = DEM+depth is approximated as water_level on wet
// cells (depth already encodes surge_level - elev).
struct StormSurgeWaterMesh {
  std::vector<double> xyz;    // interleaved map-CRS x,y,z
  std::vector<int> indices;   // triangle vertex indices (3 per tri)
};

// Build a downsampled quad mesh (2 tris per wet block). max_dim caps the
// longer axis sample count (default 96, peer of flood mask paint).
GIS_EXPORT StormSurgeWaterMesh build_storm_surge_water_mesh(
    const unsigned char* mask,
    int width,
    int height,
    const double* geotransform,
    double water_level,
    const float* depth = nullptr,
    int max_dim = 96);

// Prefer last frame when frame_index < 0.
GIS_EXPORT StormSurgeWaterMesh build_storm_surge_water_mesh(
    const StormSurgeResult& result,
    int frame_index = -1,
    int max_dim = 96);

// JSON: dem, coast|shoreline, output; surge_levels[] | tide|series|surge_series path
// | water_level | water_depth; optional frames, frames_dir, seed_x/seed_y,
// depth_output, polygons_output.
GIS_EXPORT bool run_storm_surge_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_DEM_STORM_SURGE_H_
