// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/primitive/surface/surface_base.h"

#include "scenic/render/err.h"

namespace scenic {
namespace detail {

SurfaceObject::~SurfaceObject() { release_gpu_buffers(); }

void SurfaceObject::release_gpu_buffers() {
  vb_.reset();
  ib_.reset();
  index_count_ = 0;
}

long SurfaceObject::Destroy() {
  release_gpu_buffers();
  return kErrNone;
}

bool SurfaceObject::Select(LP3DRENDERDEVICE device, const lPoint& point) {
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

bool SurfaceObject::upload_lit_mesh(LP3DRENDERDEVICE device,
                                    std::span<const float> xyz,
                                    std::span<const float> nrm,
                                    std::span<const float> rgb,
                                    std::span<const unsigned> indices) {
  release_gpu_buffers();
  const std::size_t nvert = xyz.size() / 3;
  if (!device || nvert < 1 || indices.size() < 3) {
    return false;
  }

  ulong fmt = VF_XYZ;
  if (!nrm.empty()) {
    fmt |= VF_NORMAL;
  }
  if (!rgb.empty()) {
    fmt |= VF_DIFFUSE;
  }
  vb_.reset(device->CreateVertexBuffer(static_cast<int>(nvert), fmt, false));
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
  if (!nrm.empty() && nrm.size() >= nvert * 3) {
    for (std::size_t i = 0; i < nvert; ++i) {
      const std::size_t o = i * 3;
      vb_->Normal(nrm[o], nrm[o + 1], nrm[o + 2]);
    }
  }
  if (!rgb.empty() && rgb.size() >= nvert * 3) {
    for (std::size_t i = 0; i < nvert; ++i) {
      const std::size_t o = i * 3;
      vb_->Diffuse(rgb[o], rgb[o + 1], rgb[o + 2], 1.f);
    }
  }
  vb_->Unlock();
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) * 0.5f;
  center_ = m_aAbb.vcCenter;

  ib_.reset(device->CreateIndexBuffer(static_cast<int>(indices.size())));
  if (!ib_) {
    release_gpu_buffers();
    return false;
  }
  ib_->Lock();
  for (unsigned ix : indices) {
    ib_->Index(static_cast<int>(ix));
  }
  ib_->Unlock();
  index_count_ = static_cast<ulong>(indices.size());
  return true;
}

bool SurfaceObject::upload_points(LP3DRENDERDEVICE device,
                                  std::span<const float> xyz,
                                  std::span<const float> rgba) {
  release_gpu_buffers();
  const std::size_t nvert = xyz.size() / 3;
  if (!device || nvert < 1) {
    return false;
  }
  ulong fmt = VF_XYZ;
  if (!rgba.empty()) {
    fmt |= VF_DIFFUSE;
  }
  vb_.reset(device->CreateVertexBuffer(static_cast<int>(nvert), fmt, false));
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
  if (!rgba.empty() && rgba.size() >= nvert * 4) {
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

void SurfaceObject::draw_indexed_triangles(LP3DRENDERDEVICE device) const {
  if (!device || !has_indexed_mesh()) {
    return;
  }
  device->DrawIndexedPrimitives(PT_TRIANGLELIST, vb_.get(), ib_.get(), 0,
                                index_count_ / 3);
}

}  // namespace detail
}  // namespace scenic
