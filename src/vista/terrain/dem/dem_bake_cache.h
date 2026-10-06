// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_DEM_BAKE_CACHE_H_
#define VISTA_TERRAIN_DEM_DEM_BAKE_CACHE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "vista/vista_export.h"

namespace vista {

class DemRaster;

// Public face for DEM bake caches. Implementation lives under dem/cache/:
//   io + io_pipeline — mogu-style sequential file_load + chunk-steal copies
//   phase            — load/tess/hypso timing samples
//   raster_cache / hypso_cache / mesh_cache / view_seed_cache

// Resolve cache root and create dem_bake/ once (cheap after first call).
// Scene3d seed may call this during present warmup to hide first mkdir.
VISTA_EXPORT void dem_bake_cache_warmup();

// Last DEM seed-phase clocks (load / tess / hypso) for cold attribution.
struct DemPhaseSample {
  int64_t load_ms = 0;
  int64_t tess_ms = 0;
  int64_t hypso_ms = 0;
  int load_cache_hit = 0;
  int hypso_cache_hit = 0;
};

VISTA_EXPORT DemPhaseSample dem_last_phase_sample();
VISTA_EXPORT void reset_dem_phase_sample();
VISTA_EXPORT void note_dem_phase_load(int64_t ms, bool cache_hit);
VISTA_EXPORT void note_dem_phase_tess(int64_t ms);
VISTA_EXPORT void note_dem_phase_hypso(int64_t ms, bool cache_hit);

// Process + disk cache for GDAL DEM grids (path + file stamp).
VISTA_EXPORT bool dem_raster_cache_try_get(const char* path, DemRaster* out);
VISTA_EXPORT void dem_raster_cache_put(const char* path, const DemRaster& dem);

// Process + disk cache for hypsometric RGBA (path + max_edge + file stamp).
VISTA_EXPORT bool dem_hypso_cache_try_get(const char* path, int max_edge,
                                          std::vector<uint8_t>* rgba, int* out_w,
                                          int* out_h);
VISTA_EXPORT void dem_hypso_cache_put(const char* path, int max_edge,
                                      const std::vector<uint8_t>& rgba, int w,
                                      int h);

// Tessellated DEM mesh (path + max_edge + optional window + land-mask flag).
VISTA_EXPORT bool dem_mesh_cache_try_get(const char* path, int max_edge,
                                         double minx, double miny, double maxx,
                                         double maxy, bool windowed,
                                         bool apply_land_mask,
                                         std::vector<float>* xyz,
                                         std::vector<uint32_t>* indices,
                                         std::vector<float>* uvs);
VISTA_EXPORT void dem_mesh_cache_put(const char* path, int max_edge, double minx,
                                     double miny, double maxx, double maxy,
                                     bool windowed, bool apply_land_mask,
                                     const std::vector<float>& xyz,
                                     const std::vector<uint32_t>& indices,
                                     const std::vector<float>& uvs);

// Orbit-normalized view seed (path + LOD + lon/lat frame + orbit bucket).
// Skips DEM load / tess / hypso / normalize on cold hit for Scene3d present.
struct DemViewSeed {
  float elev_cy = 0.f;
  float min_x = 0.f;
  float min_y = 0.f;
  float min_z = 0.f;
  float max_x = 0.f;
  float max_y = 0.f;
  float max_z = 0.f;
  uint32_t tex_w = 0;
  uint32_t tex_h = 0;
  std::vector<float> xyz;
  std::vector<uint32_t> indices;
  std::vector<float> uvs;
  std::vector<uint8_t> rgba;
};

VISTA_EXPORT bool dem_view_seed_cache_try_get(const char* path, int lod_key,
                                              double minx, double miny,
                                              double maxx, double maxy,
                                              DemViewSeed* out);
VISTA_EXPORT void dem_view_seed_cache_put(const char* path, int lod_key,
                                          double minx, double miny, double maxx,
                                          double maxy, const DemViewSeed& seed);

}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_DEM_BAKE_CACHE_H_
