// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Terrain payload writers plus the sample-DEM view rebuild used by present.
// Stamp / thin / window helpers stay in vista::detail. Seed hosts include
// seed.h. The Scene3d scheduler includes this header for rebuild_dem_view_mesh.

#ifndef VISTA_COMPONENT_WORLD_TERRAIN_MESH_H_
#define VISTA_COMPONENT_WORLD_TERRAIN_MESH_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/component/world/terrain/grid.h"
#include "vista/component/world/terrain/policy.h"
#include "vista/component/world/world.h"
#include "vista/mesh/tessellate.h"
#include "vista/terrain/dem/raster/dem_raster.h"
#include "vista/vista_export.h"

namespace vista {

// Caller-owned orbit hooks. Vista never includes the content geo frame.
// Null function pointers skip that step (raw DEM xyz stays in the node).
struct DemViewMeshHooks {
  // Mid elevation of the first raw tile. The return is stored on the view seed.
  float (*capture_elev_center)(const std::vector<float>& positions,
                               void* ctx) = nullptr;
  // In-place normalize of one tile's positions before AABB and concat.
  void (*normalize_xyz)(std::vector<float>* positions, void* ctx) = nullptr;
  void* ctx = nullptr;
};

// Outcome of one sample-DEM rebuild into World and the paint buffers.
struct DemViewMeshResult {
  bool painted = false;
  bool seed_cache_hit = false;
  int cache_key = 0;
  float elev_cy = 0.f;
};

// Replace kTerrain nodes for the lon/lat window and concatenate them into
// |xyz| / |idx|. Orbit normalize stays in |hooks|.
VISTA_EXPORT DemViewMeshResult rebuild_dem_view_mesh(
    World* world, double xmin, double ymin, double xmax, double ymax,
    float orbit_distance, std::vector<float>* xyz, std::vector<unsigned>* idx,
    const DemViewMeshHooks* hooks);

namespace detail {

void stamp_payload_meta(World* world, uint64_t node_id, int lod_key,
                        TerrainSource source);

void apply_elevation_overlay_texture(World* world, uint64_t node_id,
                                     const DemRaster& dem, int edge,
                                     std::vector<uint8_t>* rgba, int tw,
                                     int th);

// Prefer china_rs / map orthophoto drape; fall back to hypsometric + jet
// isoline overlay when imagery is missing.
bool apply_terrain_albedo_texture(World* world, uint64_t node_id,
                                  const DemRaster& dem, int edge);

bool thin_tess_mesh(const TessMesh& in, int stride, TessMesh* out);

bool thin_tess_mesh_spatial(const TessMesh& in, float camera_distance,
                            float cx, float cy, float cz, TessMesh* out);

bool aabb_from_xyz(const std::vector<float>& xyz, double* min_x, double* min_y,
                   double* min_z, double* max_x, double* max_y, double* max_z);

Node* attach_payload_mesh(World* world, const char* name,
                          const std::vector<float>& xyz,
                          const std::vector<uint32_t>& idx, int lod_key,
                          TerrainSource source);

Node* seed_dem_window_node(World* world, const DemRaster& dem, double tminx,
                           double tminy, double tmaxx, double tmaxy, int edge,
                           bool apply_land_mask, int lod_key,
                           TerrainSource source, const char* name);

void apply_nested_patch_continuity(World* world, Node* node,
                                   const NestedGridTile& tile);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_TERRAIN_MESH_H_
