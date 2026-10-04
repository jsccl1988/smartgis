// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_MESH_CUBE_H_
#define SCENIC_SCENE3D_PRIMITIVE_MESH_CUBE_H_

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

// Axis-aligned cube drawn via device DrawCube3D (no GPU vertex upload).
class SCENIC_IMPL_EXPORT Cube : public Object3d {
 public:
  Cube(::base::Vector3 center, float width);
  ~Cube() override;

  long Init(::base::Vector3& pos, Material& material,
            const char* tex_name = "") override;
  long Create(LP3DRENDERDEVICE device) override;
  long Render(LP3DRENDERDEVICE device) override;
  long Update(LP3DRENDERDEVICE device, float elapsed) override;
  long Destroy() override;

 private:
  Vector3 center_;
  float width_ = 1.f;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_PRIMITIVE_MESH_CUBE_H_
