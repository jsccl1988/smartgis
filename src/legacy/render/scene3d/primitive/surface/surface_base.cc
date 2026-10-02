// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/primitive/surface/surface_base.h"

#include "legacy/core/macros/macros.h"

namespace render {

SmtSurfaceObject::~SmtSurfaceObject() { release_gpu_buffers(); }

void SmtSurfaceObject::release_gpu_buffers() {
  SMT_SAFE_DELETE(vb_);
  SMT_SAFE_DELETE(ib_);
  index_count_ = 0;
}

long SmtSurfaceObject::Destroy() {
  release_gpu_buffers();
  return SMT_ERR_NONE;
}

bool SmtSurfaceObject::Select(LP3DRENDERDEVICE device, const lPoint& point) {
  if (!device || !vb_) {
    return false;
  }
  Vector3 origin;
  Vector3 target;
  device->Transform2DTo3D(origin, target, point);
  Vector3 dir = target - origin;
  if (dir.length_squared() <= 0.f) {
    return false;
  }
  Ray ray;
  float hit = 0.f;
  ray.set(origin, dir);
  return ray.intersects(m_aAbb, &hit);
}

bool SmtSurfaceObject::upload_lit_mesh(LP3DRENDERDEVICE device,
                                       const float* xyz, std::size_t nvert,
                                       const float* nrm, const float* rgb,
                                       const unsigned* indices,
                                       std::size_t nidx) {
  release_gpu_buffers();
  if (!device || !xyz || nvert < 1 || !indices || nidx < 3) {
    return false;
  }

  ulong fmt = VF_XYZ;
  if (nrm) {
    fmt |= VF_NORMAL;
  }
  if (rgb) {
    fmt |= VF_DIFFUSE;
  }
  vb_ = device->CreateVertexBuffer(static_cast<int>(nvert), fmt, false);
  if (!vb_) {
    return false;
  }

  m_aAbb = Aabb{};
  vb_->Lock();
  // Independent attribute cursors: fill position stream first, then optional
  // attributes in the same vertex order (see SmtGLVertexBuffer).
  for (std::size_t i = 0; i < nvert; ++i) {
    const std::size_t o = i * 3;
    vb_->Vertex(xyz[o], xyz[o + 1], xyz[o + 2]);
    m_aAbb.merge(xyz[o], xyz[o + 1], xyz[o + 2]);
  }
  if (nrm) {
    for (std::size_t i = 0; i < nvert; ++i) {
      const std::size_t o = i * 3;
      vb_->Normal(nrm[o], nrm[o + 1], nrm[o + 2]);
    }
  }
  if (rgb) {
    for (std::size_t i = 0; i < nvert; ++i) {
      const std::size_t o = i * 3;
      vb_->Diffuse(rgb[o], rgb[o + 1], rgb[o + 2], 1.f);
    }
  }
  vb_->Unlock();
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) * 0.5f;
  center_ = m_aAbb.vcCenter;

  ib_ = device->CreateIndexBuffer(static_cast<int>(nidx));
  if (!ib_) {
    release_gpu_buffers();
    return false;
  }
  ib_->Lock();
  for (std::size_t i = 0; i < nidx; ++i) {
    ib_->Index(static_cast<int>(indices[i]));
  }
  ib_->Unlock();
  index_count_ = static_cast<ulong>(nidx);
  return true;
}

bool SmtSurfaceObject::upload_points(LP3DRENDERDEVICE device, const float* xyz,
                                     std::size_t nvert, const float* rgba) {
  release_gpu_buffers();
  if (!device || !xyz || nvert < 1) {
    return false;
  }
  ulong fmt = VF_XYZ;
  if (rgba) {
    fmt |= VF_DIFFUSE;
  }
  vb_ = device->CreateVertexBuffer(static_cast<int>(nvert), fmt, false);
  if (!vb_) {
    return false;
  }
  m_aAbb = Aabb{};
  vb_->Lock();
  for (std::size_t i = 0; i < nvert; ++i) {
    const std::size_t o = i * 3;
    vb_->Vertex(xyz[o], xyz[o + 1], xyz[o + 2]);
    m_aAbb.merge(xyz[o], xyz[o + 1], xyz[o + 2]);
  }
  if (rgba) {
    for (std::size_t i = 0; i < nvert; ++i) {
      const std::size_t o = i * 4;
      vb_->Diffuse(rgba[o], rgba[o + 1], rgba[o + 2], rgba[o + 3]);
    }
  }
  vb_->Unlock();
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) * 0.5f;
  center_ = m_aAbb.vcCenter;
  return true;
}

void SmtSurfaceObject::draw_indexed_triangles(LP3DRENDERDEVICE device) const {
  if (!device || !has_indexed_mesh()) {
    return;
  }
  device->DrawIndexedPrimitives(PT_TRIANGLELIST, vb_, ib_, 0,
                                index_count_ / 3);
}

}  // namespace render
