// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/primitive/mesh/cube.h"

#include "scenic/render/err.h"

namespace scenic {
namespace detail {

Cube::Cube(::base::Vector3 center, float width)
    : center_(center), width_(width) {}

Cube::~Cube() { Destroy(); }

long Cube::Init(::base::Vector3& pos, Material& material,
                const char* tex_name) {
  Object3d::Init(pos, material, tex_name);

  m_matMaterial.SetAmbientValue(Color(0, 0, 0));
  m_matMaterial.SetDiffuseValue(Color(.1, .3, 1));
  m_matMaterial.SetSpecularValue(Color(1, 1, 1));
  m_matMaterial.SetEmissiveValue(Color(0.1, 1, 1, 1));
  m_matMaterial.SetShininessValue(22);

  return kErrNone;
}

long Cube::Create(LP3DRENDERDEVICE device) {
  if (!device) {
    return kErrInvalidParam;
  }

  m_aAbb.vcMax = center_ + (width_ / 2.f);
  m_aAbb.vcMin = center_ - (width_ / 2.f);
  m_aAbb.vcCenter = center_;
  return kErrNone;
}

long Cube::Update(LP3DRENDERDEVICE /*device*/, float /*elapsed*/) {
  return kErrNone;
}

long Cube::Render(LP3DRENDERDEVICE device) {
  if (!device) {
    return kErrInvalidParam;
  }

  device->SetMaterial(&m_matMaterial);
  device->MatrixPush();
  device->MatrixMultiply(m_mtxWorld);
  device->MatrixPush();
  device->MatrixMultiply(m_mtxModel);
  device->DrawCube3D(center_, width_, Color(1., 0., 0., 1.));
  device->MatrixPop();
  device->MatrixPop();
  return kErrNone;
}

long Cube::Destroy() { return kErrNone; }

}  // namespace detail
}  // namespace scenic
