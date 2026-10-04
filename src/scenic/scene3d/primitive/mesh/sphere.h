// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_MESH_SPHERE_H_
#define SCENIC_SCENE3D_PRIMITIVE_MESH_SPHERE_H_

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/scene3d/primitive/mesh/mesh_gpu.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

// UV-sphere as triangle strips in one leftover vertex buffer.
class SCENIC_IMPL_EXPORT Sphere : public Object3d {
 public:
  Sphere(float radius, unsigned slices);
  ~Sphere() override;

  long Init(::base::Vector3& pos, Material& material,
            const char* tex_name = "") override;
  long Create(LP3DRENDERDEVICE device) override;
  long Update(LP3DRENDERDEVICE device, float elapsed) override;
  long Render(LP3DRENDERDEVICE device) override;
  long Destroy() override;
  bool Select(LP3DRENDERDEVICE device, const lPoint& point) override;

  void set_x_scale(float scale) { x_scale_ = scale; }
  void set_y_scale(float scale) { y_scale_ = scale; }
  void set_z_scale(float scale) { z_scale_ = scale; }
  float x_scale() const { return x_scale_; }
  float y_scale() const { return y_scale_; }
  float z_scale() const { return z_scale_; }

 private:
  GpuVertexBuffer vb_;
  float radius_ = 1.f;
  unsigned slices_ = 8;
  float z_scale_ = 1.f;
  float x_scale_ = 1.f;
  float y_scale_ = 1.f;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_PRIMITIVE_MESH_SPHERE_H_
