// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_SCENE_LEFTOVER_MESH_H_
#define RENDER_SCENE_LEFTOVER_MESH_H_

#include <cstddef>
#include <cstdint>

#include "render/rhi/rhi.h"

namespace render {
class SmtVertexBuffer;
class SmtIndexBuffer;
}  // namespace render

// Copies leftover SmtVertexBuffer / SmtIndexBuffer CPU bytes onto the same
// render::rhi::Device used for 2D GIS meshes, then records draw_indexed.
// Does not change the leftover Smt_* ABI and does not need a GL context.

namespace render {
namespace scene {

// GPU copies of one leftover (or CPU xyz) mesh. Caller owns via destroy_*.
struct LeftoverGpuMesh {
  render::rhi::Buffer* vertex = nullptr;
  render::rhi::Buffer* index = nullptr;
  uint32_t index_count = 0;
  uint32_t stride = 0;
};

// Host leftover VB/IB matching 3d render defs (VF_XYZ / VF_XYZRHW). No GL.
render::SmtVertexBuffer* create_host_vertex_buffer(int count, uint32_t format);
void destroy_host_vertex_buffer(render::SmtVertexBuffer* vb);
render::SmtIndexBuffer* create_host_index_buffer(int count);
void destroy_host_index_buffer(render::SmtIndexBuffer* ib);

bool upload_leftover_buffers(render::rhi::Device* device,
                             render::SmtVertexBuffer* vb,
                             render::SmtIndexBuffer* ib, LeftoverGpuMesh* out);

// Packed xyz triples (same layout as tessellate TessMesh positions).
bool upload_xyz_mesh(render::rhi::Device* device, const float* xyz,
                     size_t xyz_floats, const uint32_t* indices,
                     size_t index_count, LeftoverGpuMesh* out);

// Bind + draw_indexed. Does not begin/end a pass or close the list.
bool record_leftover_draw(render::rhi::CommandList* list,
                          const LeftoverGpuMesh& mesh);

void destroy_leftover_mesh(render::rhi::Device* device, LeftoverGpuMesh* mesh);

}  // namespace scene
}  // namespace render

#endif  // RENDER_SCENE_LEFTOVER_MESH_H_
