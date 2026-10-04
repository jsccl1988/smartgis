// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_MESH_NORTHARRAY_H_
#define SCENIC_SCENE3D_PRIMITIVE_MESH_NORTHARRAY_H_

#include <memory>

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/camera/camera.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/render/rhi3d/public/resource/video_buffer.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

// Screen-space compass rose (north heading) in the 3D viewport corner.
class SCENIC_IMPL_EXPORT NorthArray : public Object3d {
 public:
  NorthArray(float init_angle, float win_h, PerspCamera* camera);
  ~NorthArray() override;

  long Init(::base::Vector3& pos, Material& material,
            const char* tex_name = "") override;
  long Create(LP3DRENDERDEVICE device) override;
  long Update(LP3DRENDERDEVICE device, float elapsed) override;
  long Render(LP3DRENDERDEVICE device) override;
  long Destroy() override;

  void set_camera(PerspCamera* camera) { camera_ = camera; }

 private:
  void draw_clock(LP3DRENDERDEVICE device);
  void draw_array(LP3DRENDERDEVICE device);

  PerspCamera* camera_ = nullptr;
  float north_pt_angle_ = 0.f;
  float win_h_ = 0.f;
  uint font_clock_ = 0;
  std::unique_ptr<VertexBuffer> vb_clock_pan_;
  std::unique_ptr<VertexBuffer> vb_clock_array_;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_PRIMITIVE_MESH_NORTHARRAY_H_
