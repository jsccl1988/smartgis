// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/primitive/mesh/mesh_gpu.h"

#include "base/math/math.h"

namespace scenic {
namespace detail {

void bind_lit_mesh(LP3DRENDERDEVICE device, Material* material,
                   const std::string& tex_name, const ::base::Matrix& world,
                   const ::base::Matrix& model) {
  if (!device) {
    return;
  }
  if (Texture* tex = device->GetTexture(tex_name.c_str())) {
    device->SetTexture(tex);
  }
  if (material) {
    device->SetMaterial(material);
  }
  device->MatrixPush();
  device->MatrixMultiply(world);
  device->MatrixPush();
  device->MatrixMultiply(model);
}

void unbind_lit_mesh(LP3DRENDERDEVICE device) {
  if (!device) {
    return;
  }
  device->MatrixPop();
  device->MatrixPop();
}

bool pick_aabb_ray(LP3DRENDERDEVICE device, const ::base::Matrix& world,
                   const ::base::Matrix& model, const ::base::Aabb& aabb,
                   const lPoint& point) {
  if (!device) {
    return false;
  }
  Vector3 origin;
  Vector3 target;
  device->MatrixPush();
  device->MatrixMultiply(world);
  device->MatrixPush();
  device->MatrixMultiply(model);
  device->Transform2DTo3D(origin, target, point);
  device->MatrixPop();
  device->MatrixPop();

  Vector3 dir = target - origin;
  if (dir.length_squared() <= 0.f) {
    return false;
  }
  Ray ray;
  float hit = 0.f;
  ray.set(origin, dir);
  return ray.intersects(aabb, &hit);
}

}  // namespace detail
}  // namespace scenic
