// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/seed.h"

#include "ogrsf_frmts.h"
#include "vista/component/world/terrain/mesh.h"
#include "vista/component/world/terrain/policy.h"
#include "vista/mesh/tessellate.h"

namespace vista {
using detail::attach_payload_mesh;
using detail::thin_tess_mesh;
using detail::thin_tess_mesh_spatial;

Node* seed_tin_into_world(World* world, const OGRTriangulatedSurface* tin,
                          const char* name) {
  return seed_tin_lod_into_world(world, tin, name, /*camera_distance=*/0.f);
}
Node* seed_tin_lod_into_world(World* world, const OGRTriangulatedSurface* tin,
                              const char* name, float camera_distance) {
  if (!world || !tin || tin->IsEmpty()) {
    return nullptr;
  }
  TessMesh full;
  // Keep Z so TIN elevations survive into TerrainPayload.
  if (!tessellate_3d_surface(tin, full) || full.indices.size() < 3) {
    return nullptr;
  }
  const int stride = terrain_lod_tin_stride(camera_distance);
  TessMesh thinned;
  if (stride <= 1) {
    thinned = std::move(full);
  } else if (!thin_tess_mesh(full, stride, &thinned)) {
    return nullptr;
  }
  const int lod_key = terrain_lod_tin_cache_key(camera_distance);
  return attach_payload_mesh(world, name, thinned.positions, thinned.indices,
                             lod_key, TerrainSource::kTin);
}
Node* seed_tin_lod_into_world(World* world, const OGRTriangulatedSurface* tin,
                              const char* name, float camera_distance,
                              float camera_x, float camera_y, float camera_z) {
  if (!world || !tin || tin->IsEmpty()) {
    return nullptr;
  }
  TessMesh full;
  if (!tessellate_3d_surface(tin, full) || full.indices.size() < 3) {
    return nullptr;
  }
  TessMesh thinned;
  if (!thin_tess_mesh_spatial(full, camera_distance, camera_x, camera_y,
                              camera_z, &thinned)) {
    return nullptr;
  }
  const int lod_key =
      terrain_lod_tin_cache_key(camera_distance, camera_x, camera_y, camera_z);
  return attach_payload_mesh(world, name, thinned.positions, thinned.indices,
                             lod_key, TerrainSource::kTin);
}
Node* seed_tin_lod_into_world(World* world, const OGRTriangulatedSurface* tin,
                              const char* name, float camera_distance,
                              const ViewState& view) {
  return seed_tin_lod_into_world(world, tin, name, camera_distance,
                                 static_cast<float>(view.eye_x),
                                 static_cast<float>(view.eye_y),
                                 static_cast<float>(view.eye_z));
}
}  // namespace vista
