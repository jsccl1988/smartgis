// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/seed.h"

#include <vector>

#include "vista/component/world/terrain/mesh.h"
#include "vista/component/world/terrain/policy.h"

namespace vista {
using detail::attach_payload_mesh;
using detail::stamp_payload_meta;

Node* seed_dem_surface_into_world(World* world,
                                  const render::DemHeightField& dem,
                                  const char* name, int max_edge) {
  if (!world || dem.empty()) {
    return nullptr;
  }
  const int edge = max_edge > 1 ? max_edge : 96;
  std::vector<float> xyz;
  std::vector<unsigned> idx_u;
  std::vector<float> rgb;
  std::vector<float> nrm;
  if (!dem.build_mesh(edge, &xyz, &idx_u, &rgb, &nrm) || xyz.size() < 9 ||
      idx_u.size() < 3) {
    return nullptr;
  }
  std::vector<uint32_t> idx(idx_u.begin(), idx_u.end());
  Node* node = attach_payload_mesh(world, name, xyz, idx, edge * 10 + 1,
                                   TerrainSource::kSurface);
  return node;
}
Node* seed_dem_surface_lod_into_world(World* world,
                                      const render::DemHeightField& dem,
                                      const char* name,
                                      float camera_distance) {
  const int edge = terrain_lod_surface_edge(camera_distance);
  Node* node = seed_dem_surface_into_world(world, dem, name, edge);
  if (node) {
    stamp_payload_meta(world, node->id, terrain_lod_cache_key(camera_distance),
                       TerrainSource::kSurface);
  }
  return node;
}
}  // namespace vista
