// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/primitive/mesh/sphere.h"

#include <cmath>

#include "base/math/math.h"
#include "scenic/render/err.h"

namespace scenic {
namespace detail {

Sphere::Sphere(float radius, unsigned slices)
    : radius_(radius), slices_(slices) {}

Sphere::~Sphere() { Destroy(); }

long Sphere::Init(::base::Vector3& pos, Material& material,
                  const char* tex_name) {
  return Object3d::Init(pos, material, tex_name);
}

long Sphere::Create(LP3DRENDERDEVICE device) {
  if (!device) {
    return kErrInvalidParam;
  }

  vb_.reset(device->CreateVertexBuffer(
      2 * static_cast<int>(slices_ + 1) * static_cast<int>(slices_),
      VF_XYZ | VF_TEXCOORD | VF_NORMAL, false));
  if (!vb_) {
    return kErrFailure;
  }

  float phi = 0.f;
  const float phi_inc = -PI / static_cast<float>(slices_);
  const float theta_inc = -2.f * PI / static_cast<float>(slices_);

  if (kErrNone == vb_->Lock()) {
    for (unsigned i = 0; i < slices_; ++i) {
      float theta = 0.f;
      for (unsigned j = 0; j <= slices_; ++j) {
        Vector3 pos;
        pos.x = radius_ * std::cos(theta) * std::sin(phi);
        pos.y = radius_ * std::sin(theta) * std::sin(phi);
        pos.z = radius_ * std::cos(phi);
        pos += m_vOrgPos;
        pos.x *= x_scale_;
        pos.y *= y_scale_;
        pos.z *= z_scale_;
        Vector3 nrm = pos;
        nrm.normalize();
        nrm.negate();
        vb_->Normal(nrm.x, nrm.z, nrm.y);
        vb_->TexVertex(theta / (2.f * PI), phi / PI);
        vb_->Vertex(pos.x, pos.z, pos.y);

        pos.x = radius_ * std::cos(theta) * std::sin(phi + phi_inc);
        pos.y = radius_ * std::sin(theta) * std::sin(phi + phi_inc);
        pos.z = radius_ * std::cos(phi + phi_inc);
        pos += m_vOrgPos;
        pos.x *= x_scale_;
        pos.y *= y_scale_;
        pos.z *= z_scale_;
        nrm = pos;
        nrm.normalize();
        nrm.negate();
        vb_->Normal(nrm.x, nrm.z, nrm.y);
        vb_->TexVertex(theta / (2.f * PI), (phi + phi_inc) / PI);
        vb_->Vertex(pos.x, pos.z, pos.y);
        theta += theta_inc;
      }
      phi += phi_inc;
    }
    vb_->Unlock();
  }

  m_aAbb.vcMax.set(radius_ * x_scale_, radius_ * y_scale_, radius_ * z_scale_);
  m_aAbb.vcMin.set(-radius_ * x_scale_, -radius_ * y_scale_,
                   -radius_ * z_scale_);
  m_aAbb.vcMax += m_vOrgPos;
  m_aAbb.vcMin += m_vOrgPos;
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.f;
  return kErrNone;
}

long Sphere::Update(LP3DRENDERDEVICE /*device*/, float /*elapsed*/) {
  return kErrNone;
}

long Sphere::Render(LP3DRENDERDEVICE device) {
  if (!device || !vb_) {
    return kErrInvalidParam;
  }
  bind_lit_mesh(device, &m_matMaterial, m_strTexName, m_mtxWorld, m_mtxModel);
  for (unsigned i = 0; i < slices_; ++i) {
    device->DrawPrimitives(PT_TRIANGLESTRIP, vb_.get(),
                           i * 2 * (slices_ + 1), 2 * slices_);
  }
  unbind_lit_mesh(device);
  return kErrNone;
}

bool Sphere::Select(LP3DRENDERDEVICE device, const lPoint& point) {
  if (!device || !vb_) {
    return false;
  }
  return pick_aabb_ray(device, m_mtxWorld, m_mtxModel, m_aAbb, point);
}

long Sphere::Destroy() {
  vb_.reset();
  return kErrNone;
}

}  // namespace detail
}  // namespace scenic
