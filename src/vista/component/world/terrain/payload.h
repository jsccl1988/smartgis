// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU terrain mesh + drape for one kTerrain World node (no RHI).

#ifndef VISTA_COMPONENT_WORLD_TERRAIN_PAYLOAD_H_
#define VISTA_COMPONENT_WORLD_TERRAIN_PAYLOAD_H_

#include <cstdint>
#include <vector>

#include "vista/component/world/terrain/lod.h"

namespace vista {

// Optional CPU terrain mesh (SP4). Layout matches DemHeightField::build_mesh:
// leftover Y-up XYZ (X=-lon, elev, lat) + triangle indices. Empty = AABB-only.
// lod_key: raster uses dem_seed_cache_key; TIN uses terrain_lod_tin_cache_key.
struct TerrainPayload {
  std::vector<float> positions;
  std::vector<uint32_t> indices;
  // Per-vertex DEM UVs (u,v) matching rgba grid. Empty = AABB UV.
  std::vector<float> uvs;
  // Optional RGBA8 terrain drape (China RS / hypsometric bake). Size =
  // tex_w * tex_h * 4. Empty = untextured lit solid.
  std::vector<uint8_t> rgba;
  uint32_t tex_w = 0;
  uint32_t tex_h = 0;
  int lod_key = 0;
  TerrainSource source = TerrainSource::kUnknown;

  bool has_mesh() const {
    return positions.size() >= 9 && (positions.size() % 3) == 0 &&
           indices.size() >= 3 && (indices.size() % 3) == 0;
  }

  bool has_texture() const {
    return !rgba.empty() && tex_w > 0 && tex_h > 0 &&
           rgba.size() >= static_cast<size_t>(tex_w) *
                              static_cast<size_t>(tex_h) * 4u;
  }

  void clear_mesh() {
    positions.clear();
    indices.clear();
    uvs.clear();
  }

  void clear_texture() {
    rgba.clear();
    tex_w = 0;
    tex_h = 0;
  }
};

}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_TERRAIN_PAYLOAD_H_
