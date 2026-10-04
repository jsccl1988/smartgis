// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_MESH_WATER_H_
#define SCENIC_SCENE3D_PRIMITIVE_MESH_WATER_H_

#include <array>

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/scene3d/primitive/mesh/mesh_gpu.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

struct WaterVertex {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  float nx = 0.f;
  float ny = 0.f;
  float nz = 0.f;
  float u = 0.f;
  float v = 0.f;
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
};

// Procedural height-field water: pressure sim on CPU, triangle-strip VB.
class SCENIC_IMPL_EXPORT Water : public Object3d {
 public:
  static constexpr int kGridWidth = 128;
  static constexpr int kGridHeight = 128;
  static constexpr int kGridSize = kGridWidth * kGridHeight;

  Water();
  ~Water() override;

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

 protected:
  void compute(float dt = 0.1f);
  void update_vertex();
  void update_normal();
  void update_texcoord();
  void update_vb();

 private:
  using Grid = std::array<std::array<double, kGridHeight>, kGridWidth>;

  GpuVertexBuffer vb_;
  float tex_offset_ = 0.f;
  float z_scale_ = 1.f;
  float x_scale_ = 1.f;
  float y_scale_ = 1.f;
  Grid pressure_{};
  Grid vel_x_{};
  Grid vel_y_{};
  Grid acc_x_{};
  Grid acc_y_{};
  std::array<WaterVertex, kGridSize> vertices_{};
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_PRIMITIVE_MESH_WATER_H_
