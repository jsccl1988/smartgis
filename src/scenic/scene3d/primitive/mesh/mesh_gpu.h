// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_MESH_MESH_GPU_H_
#define SCENIC_SCENE3D_PRIMITIVE_MESH_MESH_GPU_H_

#include <memory>
#include <string>

#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/render/rhi3d/public/resource/index_buffer.h"
#include "scenic/render/rhi3d/public/resource/video_buffer.h"

namespace scenic {
namespace detail {

using GpuVertexBuffer = std::unique_ptr<VertexBuffer>;
using GpuIndexBuffer = std::unique_ptr<IndexBuffer>;

// Bind material/texture and push world then model for a leftover Object3d.
void bind_lit_mesh(LP3DRENDERDEVICE device, Material* material,
                   const std::string& tex_name, const ::base::Matrix& world,
                   const ::base::Matrix& model);
void unbind_lit_mesh(LP3DRENDERDEVICE device);

bool pick_aabb_ray(LP3DRENDERDEVICE device, const ::base::Matrix& world,
                   const ::base::Matrix& model, const ::base::Aabb& aabb,
                   const lPoint& point);

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_PRIMITIVE_MESH_MESH_GPU_H_
