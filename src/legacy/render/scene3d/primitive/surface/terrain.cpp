// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/primitive/surface/terrain.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "base/math/math.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/rhi3d/public/state/states_manager.h"
#include "legacy/render/rhi3d/public/texture/texture_manager.h"

namespace render {
namespace {

// 256: smoother coast silhouette than 192 without DEM LOD blowup.
constexpr int kDemMeshStride = 256;

struct CachedPoint {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
};

bool env_skip_terrain() {
  const char* skip = std::getenv("SMT_RHI3D_SKIP_TERRAIN");
  return skip && (skip[0] == '1' || skip[0] == 'y' || skip[0] == 'Y');
}

SmtColor make_rgb(float r, float g, float b) {
  SmtColor c;
  c.fRed = r;
  c.fGreen = g;
  c.fBlue = b;
  c.fA = 1.f;
  return c;
}

}  // namespace

SmtTerrain::SmtTerrain() = default;

SmtTerrain::~SmtTerrain() {
  Destroy();
  owned_field_.reset();
  field_ = nullptr;
}

long SmtTerrain::Init(::base::Vector3& vPos, SmtMaterial& matMaterial,
                      const char* szTexName) {
  Smt3DObject::Init(vPos, matMaterial, szTexName);
  color_ramp_[0] = make_rgb(1.f, 0.f, 0.f);
  color_ramp_[1] = make_rgb(0.f, 1.f, 0.f);
  color_ramp_[2] = make_rgb(0.f, 0.f, 1.f);
  return SMT_ERR_NONE;
}

void SmtTerrain::set_height_field(const DemHeightField* field) {
  owned_field_.reset();
  field_ = field;
  SMT_SAFE_DELETE(surface_);
}

void SmtTerrain::adopt_height_field(DemHeightField* field) {
  owned_field_.reset(field);
  field_ = owned_field_.get();
  SMT_SAFE_DELETE(surface_);
}

const DemHeightField* SmtTerrain::height_field() const { return field_; }

long SmtTerrain::SetTerrainSurf(geo::Smt3DSurface* surf) {
  if (!surf) {
    return SMT_ERR_INVALID_PARAM;
  }
  return SetTerrainSurfDirectly(
      static_cast<geo::Smt3DSurface*>(surf->clone()));
}

long SmtTerrain::SetTerrainSurfDirectly(geo::Smt3DSurface* surf) {
  Destroy();
  owned_field_.reset();
  field_ = nullptr;
  surface_ = surf;
  if (!surface_) {
    return SMT_ERR_INVALID_PARAM;
  }

  OGREnvelope3D env;
  surface_->get_envelope(&env);
  // Leftover Y-up: map Z elevation → view Y, map Y → view Z.
  m_aAbb.vcMax.set(static_cast<float>(env.MaxX), static_cast<float>(env.MaxZ),
                   static_cast<float>(env.MaxY));
  m_aAbb.vcMin.set(static_cast<float>(env.MinX), static_cast<float>(env.MinZ),
                   static_cast<float>(env.MinY));
  m_aAbb.vcCenter = (m_aAbb.vcMin + m_aAbb.vcMax) * 0.5f;
  center_ = m_aAbb.vcCenter;
  min_z_ = static_cast<float>(env.MinZ);
  max_z_ = static_cast<float>(env.MaxZ);
  return SMT_ERR_NONE;
}

long SmtTerrain::Create(LP3DRENDERDEVICE device) {
  if (field_ && !field_->empty()) {
    return create_from_height_field(device);
  }
  return create_from_surface(device);
}

long SmtTerrain::create_from_height_field(LP3DRENDERDEVICE device) {
  release_gpu_buffers();
  if (!field_ || field_->empty()) {
    return SMT_ERR_INVALID_PARAM;
  }
  // Null device: keep owned height field (tests / deferred GL upload).
  if (!device) {
    return SMT_ERR_NONE;
  }

  std::vector<float> xyz;
  std::vector<unsigned> indices;
  std::vector<float> rgb;
  std::vector<float> nrm;
  if (!field_->build_mesh(kDemMeshStride, &xyz, &indices, &rgb, &nrm) ||
      xyz.empty() || indices.empty()) {
    return SMT_ERR_FAILURE;
  }

  if (!upload_lit_mesh(device, xyz.data(), xyz.size() / 3, nrm.data(),
                       rgb.data(), indices.data(), indices.size())) {
    return SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

long SmtTerrain::create_from_surface(LP3DRENDERDEVICE device) {
  if (!device || !surface_) {
    return SMT_ERR_INVALID_PARAM;
  }

  const int npoints = surface_->get_point_count();
  const int ntris = surface_->get_triangle_count();
  if (npoints < 1 || ntris < 1) {
    return SMT_ERR_FAILURE;
  }

  // Cache OGR points once — normals / colors / UVs never re-fetch.
  std::vector<CachedPoint> pts(static_cast<size_t>(npoints));
  for (int i = 0; i < npoints; ++i) {
    const OGRPoint p = surface_->get_point(i);
    CachedPoint& c = pts[static_cast<size_t>(i)];
    c.x = static_cast<float>(p.getX());
    c.y = static_cast<float>(p.getY());
    c.z = static_cast<float>(p.getZ());
  }

  std::vector<Vector4> normals(static_cast<size_t>(npoints));
  std::vector<unsigned> indices;
  indices.reserve(static_cast<size_t>(ntris) * 3);
  for (int i = 0; i < ntris; ++i) {
    const base::SmtTriangle tri = surface_->get_triangle(i);
    indices.push_back(static_cast<unsigned>(tri.a));
    indices.push_back(static_cast<unsigned>(tri.b));
    indices.push_back(static_cast<unsigned>(tri.c));
    const CachedPoint& a = pts[static_cast<size_t>(tri.a)];
    const CachedPoint& b = pts[static_cast<size_t>(tri.b)];
    const CachedPoint& c = pts[static_cast<size_t>(tri.c)];
    const Vector4 face =
        triangle_normal(Vector4(a.x, a.y, a.z), Vector4(b.x, b.y, b.z),
                        Vector4(c.x, c.y, c.z));
    normals[static_cast<size_t>(tri.a)] += face;
    normals[static_cast<size_t>(tri.b)] += face;
    normals[static_cast<size_t>(tri.c)] += face;
  }

  OGREnvelope3D env;
  surface_->get_envelope(&env);
  const float x0 = static_cast<float>(env.MinX);
  const float y0 = static_cast<float>(env.MinY);
  const float x_span =
      (std::max)(1.e-6f, static_cast<float>(env.MaxX - env.MinX));
  const float y_span =
      (std::max)(1.e-6f, static_cast<float>(env.MaxY - env.MinY));

  release_gpu_buffers();
  vb_ = device->CreateVertexBuffer(
      npoints, VF_XYZ | VF_TEXCOORD | VF_NORMAL | VF_DIFFUSE, false);
  if (!vb_) {
    return SMT_ERR_FAILURE;
  }
  ib_ = device->CreateIndexBuffer(static_cast<int>(indices.size()));
  if (!ib_) {
    release_gpu_buffers();
    return SMT_ERR_FAILURE;
  }

  vb_->Lock();
  for (const CachedPoint& p : pts) {
    // Leftover Y-up: (X, Z, Y).
    vb_->Vertex(p.x, p.z, p.y);
  }
  for (const CachedPoint& p : pts) {
    SmtColor clr;
    sample_color(p.z, &clr);
    vb_->Diffuse(clr.fRed, clr.fGreen, clr.fBlue, 0.3f);
  }
  for (const CachedPoint& p : pts) {
    vb_->TexVertex((p.x - x0) / x_span * 16.f, (p.y - y0) / y_span * 16.f);
  }
  for (int i = 0; i < npoints; ++i) {
    Vector4& n = normals[static_cast<size_t>(i)];
    n.normalize();
    // Match leftover: Normal(nx, nz, ny) in VB space.
    vb_->Normal(n.x, n.z, n.y);
  }
  vb_->Unlock();

  ib_->Lock();
  for (unsigned ix : indices) {
    ib_->Index(static_cast<int>(ix));
  }
  ib_->Unlock();
  index_count_ = static_cast<ulong>(indices.size());
  return SMT_ERR_NONE;
}

long SmtTerrain::Update(LP3DRENDERDEVICE /*device*/, float /*elapsed*/) {
  return SMT_ERR_NONE;
}

long SmtTerrain::Render(LP3DRENDERDEVICE device) {
  if (!device) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (field_ && has_indexed_mesh()) {
    return render_height_field(device);
  }
  render_surface(device);
  return SMT_ERR_NONE;
}

long SmtTerrain::render_height_field(LP3DRENDERDEVICE device) {
  if (!has_indexed_mesh()) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (env_skip_terrain()) {
    return SMT_ERR_NONE;
  }
  device->SetBackfaceCulling(RSV_CULL_NONE);
  device->SetShadeMode(RSV_SHADE_SOLID, 0, SmtColor(1.f, 1.f, 1.f, 1.f));
  if (SmtGPUStateManager* states = device->GetStateManager()) {
    states->SetLight(true);
    states->Set2DTextures(false);
    // Bias terrain so draped vectors win the depth test cleanly.
    states->DepthOffsetParams(1.0f, 2.0f);
    states->EnableDepthOffset(PM_FILL, true);
  }
  device->SetMaterial(&m_matMaterial);
  draw_indexed_triangles(device);
  if (SmtGPUStateManager* states = device->GetStateManager()) {
    states->EnableDepthOffset(PM_FILL, false);
    states->DepthOffsetParams(0.f, 0.f);
  }
  return SMT_ERR_NONE;
}

void SmtTerrain::render_surface(LP3DRENDERDEVICE device) {
  if (!has_indexed_mesh()) {
    return;
  }
  if (SmtTexture* tex = device->GetTexture(m_strTexName.c_str())) {
    device->SetTexture(tex);
  } else {
    device->SetTexture(nullptr);
  }
  device->SetMaterial(&m_matMaterial);
  // Triangles only — skip the old dual POINTLIST pass (was a per-frame tax).
  draw_indexed_triangles(device);
}

long SmtTerrain::Destroy() {
  release_gpu_buffers();
  SMT_SAFE_DELETE(surface_);
  // Retain owned_field_ / field_ so Create can rebuild DEM after Destroy.
  return SMT_ERR_NONE;
}

void SmtTerrain::sample_color(float height, SmtColor* out) const {
  if (!out) {
    return;
  }
  const float span = (std::max)(1.e-6f, max_z_ - min_z_);
  if (color_type_ == 1) {
    *out = color_ramp_[0];
    return;
  }
  if (color_type_ == 2) {
    const float t = (height - min_z_) / span;
    out->fA = 1.f;
    out->fRed =
        (color_ramp_[2].fRed - color_ramp_[0].fRed) * t + color_ramp_[0].fRed;
    out->fGreen = (color_ramp_[2].fGreen - color_ramp_[0].fGreen) * t +
                  color_ramp_[0].fGreen;
    out->fBlue = (color_ramp_[2].fBlue - color_ramp_[0].fBlue) * t +
                 color_ramp_[0].fBlue;
    return;
  }
  if (color_type_ == 3) {
    const float mid = (max_z_ + min_z_) * 0.5f;
    if (height <= mid) {
      const float t = (height - min_z_) / (std::max)(1.e-6f, mid - min_z_);
      out->fA = 1.f;
      out->fRed = (color_ramp_[1].fRed - color_ramp_[0].fRed) * t +
                  color_ramp_[0].fRed;
      out->fGreen = (color_ramp_[1].fGreen - color_ramp_[0].fGreen) * t +
                    color_ramp_[0].fGreen;
      out->fBlue = (color_ramp_[1].fBlue - color_ramp_[0].fBlue) * t +
                   color_ramp_[0].fBlue;
    } else {
      const float t = (height - mid) / (std::max)(1.e-6f, max_z_ - mid);
      out->fA = 1.f;
      out->fRed = (color_ramp_[2].fRed - color_ramp_[1].fRed) * t +
                  color_ramp_[1].fRed;
      out->fGreen = (color_ramp_[2].fGreen - color_ramp_[1].fGreen) * t +
                    color_ramp_[1].fGreen;
      out->fBlue = (color_ramp_[2].fBlue - color_ramp_[1].fBlue) * t +
                   color_ramp_[1].fBlue;
    }
  }
}

}  // namespace render
