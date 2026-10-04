// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/primitive/mesh/water.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/math/math.h"
#include "scenic/render/err.h"

namespace scenic {
namespace detail {

using ::base::Vector4;
using ::base::triangle_normal;

Water::Water() = default;

Water::~Water() { Destroy(); }

long Water::Init(::base::Vector3& pos, Material& material,
                 const char* tex_name) {
  Object3d::Init(pos, material, tex_name);
  tex_offset_ = 0.f;

  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      const int index = y * kGridWidth + x;
      vertices_[static_cast<size_t>(index)].x =
          (x - kGridWidth / 2) * x_scale_;
      vertices_[static_cast<size_t>(index)].y =
          (y - kGridHeight / 2) * z_scale_;
      vertices_[static_cast<size_t>(index)].z = 0.f;
      const float u = static_cast<float>(x) / static_cast<float>(kGridWidth);
      const float v = static_cast<float>(y) / static_cast<float>(kGridHeight);
      vertices_[static_cast<size_t>(index)].r = 0.12f + 0.08f * u;
      vertices_[static_cast<size_t>(index)].g = 0.42f + 0.18f * v;
      vertices_[static_cast<size_t>(index)].b = 0.62f + 0.20f * (1.f - u);
      vertices_[static_cast<size_t>(index)].u = static_cast<float>(y);
      vertices_[static_cast<size_t>(index)].v = static_cast<float>(x);
      pressure_[static_cast<size_t>(x)][static_cast<size_t>(y)] = 0.0;
      vel_x_[static_cast<size_t>(x)][static_cast<size_t>(y)] = 0.0;
      vel_y_[static_cast<size_t>(x)][static_cast<size_t>(y)] = 0.0;
    }
  }

  update_vertex();
  update_normal();
  update_texcoord();
  return kErrNone;
}

long Water::Create(LP3DRENDERDEVICE device) {
  if (!device) {
    return kErrInvalidParam;
  }

  vb_.reset(device->CreateVertexBuffer(
      2 * (kGridHeight - 1) * kGridWidth,
      VF_XYZ | VF_DIFFUSE | VF_NORMAL | VF_TEXCOORD, false));
  if (!vb_) {
    return kErrFailure;
  }

  m_aAbb.vcMax.set(0, 0, 0);
  m_aAbb.vcMin.set(0, 0, 0);
  m_aAbb.vcMax += (kGridHeight / 2.f) * x_scale_;
  m_aAbb.vcMin += -(kGridHeight / 2.f) * z_scale_;
  m_aAbb.vcCenter += (m_aAbb.vcMax + m_aAbb.vcMin) / 2.f;

  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      const double dx = static_cast<double>(x - kGridWidth / 2);
      const double dy = static_cast<double>(y - kGridHeight / 2);
      const double d = std::sqrt(dx * dx + dy * dy);
      const double radius = 0.35 * static_cast<double>(kGridWidth / 2);
      if (d < radius) {
        const double t = d * (PI / static_cast<double>(kGridWidth * 2));
        pressure_[static_cast<size_t>(x)][static_cast<size_t>(y)] =
            -std::cos(t) * 55.0;
      } else {
        pressure_[static_cast<size_t>(x)][static_cast<size_t>(y)] =
            8.0 * std::sin(dx * 0.35) * std::cos(dy * 0.28);
      }
      vel_x_[static_cast<size_t>(x)][static_cast<size_t>(y)] = 0.0;
      vel_y_[static_cast<size_t>(x)][static_cast<size_t>(y)] = 0.0;
    }
  }

  update_vertex();
  update_normal();
  update_texcoord();
  update_vb();
  return kErrNone;
}

long Water::Update(LP3DRENDERDEVICE /*device*/, float elapsed) {
  const float dt = (std::max)(0.001f, (std::min)(elapsed, 0.05f));
  compute(dt);
  update_vertex();
  update_normal();
  update_texcoord();
  update_vb();
  return kErrNone;
}

long Water::Render(LP3DRENDERDEVICE device) {
  if (!device || !vb_) {
    return kErrInvalidParam;
  }
  bind_lit_mesh(device, &m_matMaterial, m_strTexName, m_mtxWorld, m_mtxModel);
  for (int i = 0; i < kGridHeight - 1; ++i) {
    device->DrawPrimitives(PT_TRIANGLESTRIP, vb_.get(),
                           i * 2 * kGridWidth, 2 * (kGridWidth - 1));
  }
  unbind_lit_mesh(device);
  return kErrNone;
}

bool Water::Select(LP3DRENDERDEVICE device, const lPoint& point) {
  if (!device || !vb_) {
    return false;
  }
  return pick_aabb_ray(device, m_mtxWorld, m_mtxModel, m_aAbb, point);
}

long Water::Destroy() {
  vb_.reset();
  return kErrNone;
}

void Water::compute(float dt) {
  const double time_step = dt * 1.0;

  for (int x = 0; x < kGridWidth; ++x) {
    const int x2 = (x + 1) % kGridWidth;
    for (int y = 0; y < kGridHeight; ++y) {
      acc_x_[static_cast<size_t>(x)][static_cast<size_t>(y)] =
          pressure_[static_cast<size_t>(x)][static_cast<size_t>(y)] -
          pressure_[static_cast<size_t>(x2)][static_cast<size_t>(y)];
    }
  }
  for (int y = 0; y < kGridHeight; ++y) {
    const int y2 = (y + 1) % kGridHeight;
    for (int x = 0; x < kGridWidth; ++x) {
      acc_y_[static_cast<size_t>(x)][static_cast<size_t>(y)] =
          pressure_[static_cast<size_t>(x)][static_cast<size_t>(y)] -
          pressure_[static_cast<size_t>(x)][static_cast<size_t>(y2)];
    }
  }
  for (int x = 0; x < kGridWidth; ++x) {
    for (int y = 0; y < kGridHeight; ++y) {
      vel_x_[static_cast<size_t>(x)][static_cast<size_t>(y)] +=
          acc_x_[static_cast<size_t>(x)][static_cast<size_t>(y)] * time_step;
      vel_y_[static_cast<size_t>(x)][static_cast<size_t>(y)] +=
          acc_y_[static_cast<size_t>(x)][static_cast<size_t>(y)] * time_step;
    }
  }
  for (int x = 1; x < kGridWidth; ++x) {
    const int x2 = x - 1;
    for (int y = 1; y < kGridHeight; ++y) {
      const int y2 = y - 1;
      pressure_[static_cast<size_t>(x)][static_cast<size_t>(y)] +=
          (vel_x_[static_cast<size_t>(x2)][static_cast<size_t>(y)] -
           vel_x_[static_cast<size_t>(x)][static_cast<size_t>(y)] +
           vel_y_[static_cast<size_t>(x)][static_cast<size_t>(y2)] -
           vel_y_[static_cast<size_t>(x)][static_cast<size_t>(y)]) *
          time_step;
    }
  }
}

void Water::update_vertex() {
  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      const int index = y * kGridWidth + x;
      vertices_[static_cast<size_t>(index)].z =
          static_cast<float>(
              pressure_[static_cast<size_t>(x)][static_cast<size_t>(y)] *
              0.1) *
          y_scale_;
    }
  }
}

void Water::update_normal() {
  std::vector<Vector3> normals(static_cast<size_t>(kGridSize));
  for (int iy = 0; iy < kGridHeight - 1; ++iy) {
    for (int ix = 0; ix < kGridWidth - 1; ++ix) {
      const int p1 = (iy * kGridWidth) + ix;
      const int p2 = ((iy + 1) * kGridWidth) + ix;
      const int p3 = p1 + 1;
      const int p4 = p2 + 1;
      const WaterVertex& a = vertices_[static_cast<size_t>(p1)];
      const WaterVertex& b = vertices_[static_cast<size_t>(p2)];
      const WaterVertex& c = vertices_[static_cast<size_t>(p3)];
      const WaterVertex& d = vertices_[static_cast<size_t>(p4)];
      const Vector4 v1(a.x, a.y, a.z);
      const Vector4 v2(b.x, b.y, b.z);
      const Vector4 v3(c.x, c.y, c.z);
      const Vector4 v4(d.x, d.y, d.z);
      const Vector4 nor1 = triangle_normal(v1, v3, v2);
      const Vector4 nor2 = triangle_normal(v3, v4, v2);
      normals[static_cast<size_t>(p1)] += nor1;
      normals[static_cast<size_t>(p2)] += nor1;
      normals[static_cast<size_t>(p3)] += nor1;
      normals[static_cast<size_t>(p2)] += nor2;
      normals[static_cast<size_t>(p3)] += nor2;
      normals[static_cast<size_t>(p4)] += nor2;
    }
  }
  for (int i = 0; i < kGridSize; ++i) {
    normals[static_cast<size_t>(i)].normalize();
    vertices_[static_cast<size_t>(i)].nx = normals[static_cast<size_t>(i)].x;
    vertices_[static_cast<size_t>(i)].ny = normals[static_cast<size_t>(i)].y;
    vertices_[static_cast<size_t>(i)].nz = normals[static_cast<size_t>(i)].z;
  }
}

void Water::update_texcoord() {
  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      const int index = y * kGridWidth + x;
      vertices_[static_cast<size_t>(index)].u =
          4.f * y / static_cast<float>(kGridWidth) + tex_offset_;
      vertices_[static_cast<size_t>(index)].v =
          4.f * x / static_cast<float>(kGridHeight) + tex_offset_;
    }
  }
  tex_offset_ += 0.002f;
}

void Water::update_vb() {
  if (!vb_ || kErrNone != vb_->Lock()) {
    return;
  }
  for (int iy = 0; iy < kGridHeight - 1; ++iy) {
    for (int ix = 0; ix < kGridWidth; ++ix) {
      int p = iy * kGridWidth + ix;
      const WaterVertex& a = vertices_[static_cast<size_t>(p)];
      vb_->Vertex(a.x, a.z, a.y);
      vb_->Normal(a.nx, a.nz, a.ny);
      vb_->Diffuse(a.r, a.g, a.b, 0.85f);
      vb_->TexVertex(a.u, a.v);
      p = (iy + 1) * kGridWidth + ix;
      const WaterVertex& b = vertices_[static_cast<size_t>(p)];
      vb_->Vertex(b.x, b.z, b.y);
      vb_->Normal(b.nx, b.nz, b.ny);
      vb_->Diffuse(b.r, b.g, b.b, 0.85f);
      vb_->TexVertex(b.u, b.v);
    }
  }
  vb_->Unlock();
}

}  // namespace detail
}  // namespace scenic
