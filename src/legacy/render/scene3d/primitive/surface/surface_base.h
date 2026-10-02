// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_PRIMITIVE_SURFACE_SURFACE_BASE_H_
#define SMT_LEGACY_RENDER_SCENE3D_PRIMITIVE_SURFACE_SURFACE_BASE_H_

#include <cstddef>

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_device.h"
#include "legacy/render/rhi3d/public/resource/video_buffer.h"
#include "legacy/render/scene3d/scene/object.h"

namespace render {

// Shared leftover surface drawable: owns optional VB/IB and AABB ray Select.
// Terrain (indexed lit mesh) and pointcloud (point list) specialize Create/Render.
class LEGACY_RENDER_EXPORT SmtSurfaceObject : public Smt3DObject {
 public:
  SmtSurfaceObject() = default;
  ~SmtSurfaceObject() override;

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

  SmtVertexBuffer* vb_ = nullptr;
  SmtIndexBuffer* ib_ = nullptr;
  ulong index_count_ = 0;
  Vector3 center_;
};

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_PRIMITIVE_SURFACE_SURFACE_BASE_H_
