// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GPU-uploaded triangle mesh for one World node (or point-cloud chunk).

#ifndef VISTA_WORLD_GPU_GPU_MESH_H_
#define VISTA_WORLD_GPU_GPU_MESH_H_

#include <cstdint>

#include "render/rhi/rhi.h"
#include "vista/world/cull/mesh_cull.h"
#include "vista/world/world.h"

namespace vista {

// Lit 3D kinds (terrain/model/tileset) use stride = 6 floats
// (POSITION+NORMAL); 2D solid stays 3, textured 5.
struct GpuMesh {
  vista::NodeKind kind = vista::NodeKind::kVectorLayer;
  render::rhi::Buffer* vertex = nullptr;
  render::rhi::Buffer* index = nullptr;
  render::rhi::Texture* texture = nullptr;
  uint32_t index_count = 0;
  uint32_t stride = 0;
  float solid_r = 1.f;
  float solid_g = 1.f;
  float solid_b = 1.f;
  float solid_a = 1.f;
  // Style scalars applied at tessellate time (line ribbon / circle diamond).
  float line_width = 1.f;
  float circle_radius = 5.f;
  // World-space AABB from tessellated verts (CPU frustum cull).
  float aabb_min_x = 0.f;
  float aabb_min_y = 0.f;
  float aabb_max_x = 0.f;
  float aabb_max_y = 0.f;
  float aabb_min_z = 0.f;
  float aabb_max_z = 0.f;
};

inline MeshCullItem as_cull_item(const GpuMesh& mesh) {
  MeshCullItem item;
  item.aabb_min_x = mesh.aabb_min_x;
  item.aabb_min_y = mesh.aabb_min_y;
  item.aabb_min_z = mesh.aabb_min_z;
  item.aabb_max_x = mesh.aabb_max_x;
  item.aabb_max_y = mesh.aabb_max_y;
  item.aabb_max_z = mesh.aabb_max_z;
  item.index_count = mesh.index_count;
  item.has_buffers = mesh.vertex != nullptr && mesh.index != nullptr;
  return item;
}

}  // namespace vista

#endif  // VISTA_WORLD_GPU_GPU_MESH_H_
