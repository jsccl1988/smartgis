// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Internal terrain payload writers. Seed entry points call these;
// hosts include seed.h / policy.h / grid.h instead.

#ifndef VISTA_COMPONENT_WORLD_TERRAIN_MESH_H_
#define VISTA_COMPONENT_WORLD_TERRAIN_MESH_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/component/world/terrain/grid.h"
#include "vista/component/world/terrain/policy.h"
#include "vista/component/world/world.h"
#include "vista/mesh/tessellate.h"
#include "vista/terrain/dem/dem_raster.h"

namespace vista {
namespace detail {

void stamp_payload_meta(World* world, uint64_t node_id, int lod_key,
                        TerrainSource source);

void apply_elevation_overlay_texture(World* world, uint64_t node_id,
                                     const DemRaster& dem, int edge,
                                     std::vector<uint8_t>* rgba, int tw,
                                     int th);

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
