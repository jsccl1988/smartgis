// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_DETAIL_MESH_H_
#define EFFECT_ATMOSPHERE_DETAIL_MESH_H_

#include <cstdint>

#include "render/rhi/rhi.h"

namespace effect {
namespace atmosphere {
namespace detail {

// Axis-aligned quad on XZ at a fixed Y. Indices face +Y.
struct XzQuad {
  float xyz[12];
  uint32_t indices[6];
};

inline XzQuad make_xz_quad(float half_extent, float y) {
  const float e = half_extent;
  XzQuad quad = {
      {-e, y, -e, e, y, -e, e, y, e, -e, y, e},
      {0, 1, 2, 0, 2, 3},
  };
  return quad;
}

inline bool static_mesh_ready(const render::rhi::Device* bound, const render::rhi::Buffer* vertex,
                              const render::rhi::Buffer* index, const render::rhi::Device* device) {
  return device && bound == device && vertex && index;
}

// Drop handles without destroy_*. A shut-down FlyCube facade must not be touched.
inline void release_static_mesh(render::rhi::Device** device_slot, render::rhi::Buffer** vertex,
                                render::rhi::Buffer** index, uint32_t* index_count) {
  if (vertex) {
    *vertex = nullptr;
  }
  if (index) {
    *index = nullptr;
  }
  if (index_count) {
    *index_count = 0;
  }
  if (device_slot) {
    *device_slot = nullptr;
  }
}

// Replace a static mesh. Previous handles are dropped, not destroyed.
inline bool upload_static_mesh(render::rhi::Device** device_slot, render::rhi::Buffer** vertex,
                               render::rhi::Buffer** index, uint32_t* index_count,
                               render::rhi::Device* device, const void* vertices,
                               uint32_t vertex_bytes, const void* indices,
                               uint32_t index_bytes) {
  if (!device_slot || !vertex || !index || !device || !vertices || !indices ||
      vertex_bytes == 0 || index_bytes == 0) {
    return false;
  }
  release_static_mesh(device_slot, vertex, index, index_count);
  *device_slot = device;
  *vertex = device->create_buffer(vertex_bytes, render::rhi::BufferUsage::kVertex);
  *index = device->create_buffer(index_bytes, render::rhi::BufferUsage::kIndex);
  if (!*vertex || !*index ||
      !device->upload(*vertex, vertices, vertex_bytes) ||
      !device->upload(*index, indices, index_bytes)) {
    release_static_mesh(device_slot, vertex, index, index_count);
    return false;
  }
  if (index_count) {
    *index_count = index_bytes / static_cast<uint32_t>(sizeof(uint32_t));
  }
  return true;
}

// Destroy live ocean mesh buffers, then null the handles.
inline void destroy_mesh_buffers(render::rhi::Device* device, render::rhi::Buffer** vertex,
                                 render::rhi::Buffer** index) {
  if (device && vertex && *vertex) {
    device->destroy_buffer(*vertex);
  }
  if (vertex) {
    *vertex = nullptr;
  }
  if (device && index && *index) {
    device->destroy_buffer(*index);
  }
  if (index) {
    *index = nullptr;
  }
}

inline void draw_indexed_mesh(render::rhi::CommandList* list, render::rhi::Buffer* vertex,
                              render::rhi::Buffer* index, uint32_t stride,
                              uint32_t index_count) {
  list->bind_vertex_buffer(vertex, 0, stride);
  list->bind_index_buffer(index, 0);
  list->draw_indexed(index_count, 1, 0, 0, 0);
}

}  // namespace detail
}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_DETAIL_MESH_H_
