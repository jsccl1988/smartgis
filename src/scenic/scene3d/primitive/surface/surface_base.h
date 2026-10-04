// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_SURFACE_SURFACE_BASE_H_
#define SCENIC_SCENE3D_PRIMITIVE_SURFACE_SURFACE_BASE_H_

#include <cstddef>

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/render/rhi3d/public/resource/video_buffer.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

// Shared leftover surface drawable: owns optional VB/IB and AABB ray Select.
// Terrain (indexed lit mesh) and pointcloud (point list) specialize Create/Render.
class LEGACY_RENDER_EXPORT SurfaceObject : public Object3d {
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
    return vb_ != nullptr && ib_ != nullptr && index_count_ >= 3;
  }
  bool has_vertex_buffer() const { return vb_ != nullptr; }

  // SOA lit mesh: xyz / optional nrm / optional rgb (triplets), then indices.
  // Advances independent VB attribute cursors (GL leftover layout).
  bool upload_lit_mesh(LP3DRENDERDEVICE device, const float* xyz,
                       std::size_t nvert, const float* nrm, const float* rgb,
                       const unsigned* indices, std::size_t nidx);

  // Point list with optional per-vertex diffuse (rgba quads).
  bool upload_points(LP3DRENDERDEVICE device, const float* xyz,
                     std::size_t nvert, const float* rgba);

  void draw_indexed_triangles(LP3DRENDERDEVICE device) const;

  VertexBuffer* vb_ = nullptr;
  IndexBuffer* ib_ = nullptr;
  ulong index_count_ = 0;
  Vector3 center_;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_PRIMITIVE_SURFACE_SURFACE_BASE_H_
