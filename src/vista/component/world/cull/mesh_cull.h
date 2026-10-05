// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU-only mesh AABB + upload flags for frustum prep. No RHI handles.

#ifndef VISTA_COMPONENT_WORLD_CULL_MESH_CULL_H_
#define VISTA_COMPONENT_WORLD_CULL_MESH_CULL_H_

#include <cstdint>

namespace vista {

// One uploaded (or empty) mesh as seen by prep_cull_meshes. Workers never
// receive Device / CommandList; Buffer* stay on GpuMesh.
struct MeshCullItem {
  float aabb_min_x = 0.f;
  float aabb_min_y = 0.f;
  float aabb_min_z = 0.f;
  float aabb_max_x = 0.f;
  float aabb_max_y = 0.f;
  float aabb_max_z = 0.f;
  uint32_t index_count = 0;
  bool has_buffers = false;
};

}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_CULL_MESH_CULL_H_
