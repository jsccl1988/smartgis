// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_SURFACE_SURFACE_BASE_H_
#define SCENIC_SCENE3D_PRIMITIVE_SURFACE_SURFACE_BASE_H_

#include <cstddef>
#include <span>

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/scene3d/primitive/mesh/mesh_gpu.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

// Shared surface drawable: owns optional VB/IB and AABB ray Select.
// Terrain (indexed lit mesh) and pointcloud (point list) specialize Create/Render.
class SCENIC_IMPL_EXPORT SurfaceObject : public Object3d {
 public:
  SurfaceObject() = default;
  ~SurfaceObject() override;

  long Destroy() override;
  bool Select(LP3DRENDERDEVICE device, const lPoint& point) override;

  Vector3 center() const { return center_; }
  void set_center(const Vector3& c) { center_ = c; }

 protected:
  void release_gpu_buffers();

  bool has_indexed_mesh() const {
    return vb_ && ib_ && index_count_ >= 3;
  }
  bool has_vertex_buffer() const { return static_cast<bool>(vb_); }
  VertexBuffer* vertex_buffer() const { return vb_.get(); }

  // SOA lit mesh: xyz / optional nrm / optional rgb (triplets), then indices.
  bool upload_lit_mesh(LP3DRENDERDEVICE device, std::span<const float> xyz,
                       std::span<const float> nrm, std::span<const float> rgb,
                       std::span<const unsigned> indices);

  bool upload_points(LP3DRENDERDEVICE device, std::span<const float> xyz,
                     std::span<const float> rgba);

  void draw_indexed_triangles(LP3DRENDERDEVICE device) const;

  GpuVertexBuffer vb_;
  GpuIndexBuffer ib_;
  ulong index_count_ = 0;
  Vector3 center_;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_PRIMITIVE_SURFACE_SURFACE_BASE_H_
