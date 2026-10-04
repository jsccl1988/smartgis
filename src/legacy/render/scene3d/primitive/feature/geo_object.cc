// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/primitive/feature/geo_object.h"

#include <algorithm>
#include <cstdio>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "gis/geo/ops/geometry_traits.h"
#include "gis/envelope.h"
#include "legacy/core/types/types.h"
#include "legacy/render/rhi3d/public/state/states_manager.h"
#include "ogr_geometry.h"

namespace render {
namespace {

void release_ogr_geometry(OGRGeometry*& geom) {
  if (geom) {
    OGRGeometryFactory::destroyGeometry(geom);
    geom = nullptr;
  }
}

FeatureRgb rgb_from_colorref(COLORREF c) {
  FeatureRgb out;
  out.r = GetRValue(c) / 255.f;
  out.g = GetGValue(c) / 255.f;
  out.b = GetBValue(c) / 255.f;
  return out;
}

}  // namespace

SmtGeoObject::SmtGeoObject() = default;

SmtGeoObject::~SmtGeoObject() { Destroy(); }

void SmtGeoObject::SetHeightSampleFn(FeatureHeightSampleFn fn, void* user) {
  height_fn_ = fn;
  height_user_ = user;
}

ulong SmtGeoObject::vertex_format(const FeatureMesh& mesh) const {
  ulong fmt = VF_XYZ;
  if (mesh.has_colors) {
    fmt |= VF_DIFFUSE;
  }
  if (mesh.has_normals) {
    fmt |= VF_NORMAL;
  }
  return fmt;
}

bool SmtGeoObject::upload_mesh(LP3DRENDERDEVICE device,
                               const FeatureMesh& mesh) {
  if (!device || mesh.vertices.empty()) {
    return false;
  }
  SMT_SAFE_DELETE(vb_);
  SMT_SAFE_DELETE(ib_);

  vb_ = device->CreateVertexBuffer(static_cast<int>(mesh.vertices.size()),
                                   vertex_format(mesh), false);
  if (!vb_) {
    return false;
  }
  vb_->Lock();
  for (const FeatureVertex& v : mesh.vertices) {
    if (v.has_normal || mesh.has_normals) {
      vb_->Normal(v.nx, v.ny, v.nz);
    }
    vb_->Vertex(v.x, v.y, v.z);
    if (v.has_color || mesh.has_colors) {
      vb_->Diffuse(v.r, v.g, v.b, v.a);
    }
  }
  vb_->Unlock();

  if (mesh.indexed && !mesh.indices.empty()) {
    ib_ = device->CreateIndexBuffer(static_cast<int>(mesh.indices.size()));
    if (!ib_) {
      SMT_SAFE_DELETE(vb_);
      return false;
    }
    ib_->Lock();
    for (std::uint32_t ix : mesh.indices) {
      ib_->Index(static_cast<int>(ix));
    }
    ib_->Unlock();
  }

  prim_ = mesh.prim;
  indexed_ = mesh.indexed && ib_ != nullptr;
  return true;
}

void SmtGeoObject::update_aabb_map() {
  if (!geom_) {
    return;
  }
  gis::Envelope env;
  geo::fill_envelope(*geom_, &env);
  const float h00 =
      height_fn_ ? height_fn_(env.MinX, env.MinY, height_user_) : 0.f;
  const float h10 =
      height_fn_ ? height_fn_(env.MaxX, env.MinY, height_user_) : 0.f;
  const float h01 =
      height_fn_ ? height_fn_(env.MinX, env.MaxY, height_user_) : 0.f;
  const float h11 =
      height_fn_ ? height_fn_(env.MaxX, env.MaxY, height_user_) : 0.f;
  const float hmin = (std::min)((std::min)(h00, h10), (std::min)(h01, h11));
  const float hmax = (std::max)((std::max)(h00, h10), (std::max)(h01, h11));
  m_aAbb.merge(-env.MaxX, hmin, env.MinY);
  m_aAbb.merge(-env.MinX, hmax, env.MaxY);
  m_aAbb.vcMax += m_vOrgPos;
  m_aAbb.vcMin += m_vOrgPos;
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.;
}

void SmtGeoObject::update_aabb_world() {
  if (!geom_) {
    return;
  }
  OGREnvelope3D env;
  geo::fill_envelope3d(*geom_, &env);
  m_aAbb.vcMin.set(static_cast<float>(env.MinX), static_cast<float>(env.MinY),
                   static_cast<float>(env.MinZ));
  m_aAbb.vcMax.set(static_cast<float>(env.MaxX), static_cast<float>(env.MaxY),
                   static_cast<float>(env.MaxZ));
  m_aAbb.vcMax += m_vOrgPos;
  m_aAbb.vcMin += m_vOrgPos;
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.;
}

long SmtGeoObject::Init(::base::Vector3& vPos, SmtMaterial& matMaterial,
                        const char* szTexName) {
  return Smt3DObject::Init(vPos, matMaterial, szTexName);
}

void SmtGeoObject::update_aabb_from_mesh(const FeatureMesh& mesh) {
  for (const FeatureVertex& v : mesh.vertices) {
    m_aAbb.merge(v.x, v.y, v.z);
  }
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.f;
}

long SmtGeoObject::CreateFromMesh(LP3DRENDERDEVICE device, FeatureMesh mesh) {
  if (!device || mesh.vertices.empty()) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!upload_mesh(device, mesh)) {
    return SMT_ERR_FAILURE;
  }
  update_aabb_from_mesh(mesh);
  return SMT_ERR_NONE;
}

long SmtGeoObject::Create(LP3DRENDERDEVICE p3DRenderDevice) {
  if (!p3DRenderDevice || !geom_) {
    return SMT_ERR_INVALID_PARAM;
  }

  FeatureMesh mesh;
  bool ok = false;
  if (frame_ == GeoObjectFrame::kMap) {
    if (!style_) {
      SmtStyle fallback;
      SetStyle(&fallback);
    }
    const FeatureRgb stroke = rgb_from_colorref(style_->get_pen_desc().lPenColor);
    const FeatureRgb fill =
        rgb_from_colorref(style_->get_brush_desc().lBrushColor);
    ok = tess_map_geometry(*geom_, stroke, fill, height_fn_, height_user_,
                           &mesh);
    if (ok) {
      update_aabb_map();
    }
  } else {
    ok = tess_world_geometry(*geom_, &mesh);
    if (ok) {
      update_aabb_world();
    }
  }

  if (!ok || !upload_mesh(p3DRenderDevice, mesh)) {
    return SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

long SmtGeoObject::Update(LP3DRENDERDEVICE /*p3DRenderDevice*/,
                          float /*fElapsed*/) {
  return SMT_ERR_NONE;
}

long SmtGeoObject::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (!p3DRenderDevice || !vb_) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (frame_ == GeoObjectFrame::kMap) {
    p3DRenderDevice->SetBackfaceCulling(RSV_CULL_NONE);
    p3DRenderDevice->SetShadeMode(RSV_SHADE_SOLID, 0,
                                  SmtColor(1.f, 1.f, 1.f, 1.f));
    if (SmtGPUStateManager* states = p3DRenderDevice->GetStateManager()) {
      states->SetLight(height_fn_ != nullptr);
      states->Set2DTextures(false);
    }
  }

  SmtTexture* pTex = p3DRenderDevice->GetTexture(m_strTexName.c_str());
  if (pTex) {
    p3DRenderDevice->SetTexture(pTex);
  }
  p3DRenderDevice->SetMaterial(&m_matMaterial);

  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxWorld);
  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxModel);

  if (indexed_ && ib_) {
    const ulong nidx = ib_->GetIndexCount();
    if (nidx >= 3) {
      p3DRenderDevice->DrawIndexedPrimitives(PT_TRIANGLELIST, vb_, ib_, 0,
                                             nidx / 3);
    }
  } else {
    switch (prim_) {
      case FeaturePrim::kPoints:
        p3DRenderDevice->DrawPrimitives(PT_POINTLIST, vb_, 0,
                                        vb_->GetVertexCount());
        break;
      case FeaturePrim::kLineStrip:
        p3DRenderDevice->DrawPrimitives(PT_LINESTRIP, vb_, 0,
                                        vb_->GetVertexCount());
        break;
      case FeaturePrim::kLineList: {
        const ulong nvert = vb_->GetVertexCount();
        if (nvert >= 2) {
          p3DRenderDevice->DrawPrimitives(PT_LINELIST, vb_, 0, nvert / 2);
        }
        break;
      }
      case FeaturePrim::kTriangles:
        p3DRenderDevice->DrawPrimitives(PT_TRIANGLELIST, vb_, 0,
                                        vb_->GetVertexCount() / 3);
        break;
    }
  }

  p3DRenderDevice->MatrixPop();
  p3DRenderDevice->MatrixPop();
  return SMT_ERR_NONE;
}

bool SmtGeoObject::Select(LP3DRENDERDEVICE p3DRenderDevice,
                          const lPoint& point) {
  if (!p3DRenderDevice || !vb_) {
    return false;
  }

  Vector3 vOrg, vTar, vDir;
  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxWorld);
  p3DRenderDevice->MatrixPush();
  p3DRenderDevice->MatrixMultiply(m_mtxModel);
  p3DRenderDevice->Transform2DTo3D(vOrg, vTar, point);
  p3DRenderDevice->MatrixPop();
  p3DRenderDevice->MatrixPop();

  vDir = vTar - vOrg;
  if (vDir.length_squared() <= 0) {
    return false;
  }
  Ray ray;
  float f = 0.f;
  ray.set(vOrg, vDir);
  if (!ray.intersects(m_aAbb, &f)) {
    return false;
  }
  if (frame_ == GeoObjectFrame::kMap && geom_ &&
      wkbFlatten(geom_->getGeometryType()) == wkbPolygon) {
    OGRPoint oPoint(vTar.x, vTar.z);
    return geom_->Contains(&oPoint) != 0;
  }
  return true;
}

long SmtGeoObject::Destroy() {
  SMT_SAFE_DELETE(vb_);
  SMT_SAFE_DELETE(ib_);
  release_ogr_geometry(geom_);
  SMT_SAFE_DELETE(style_);
  return SMT_ERR_NONE;
}

void SmtGeoObject::SetGeometryDirectly(OGRGeometry* pGeom) {
  release_ogr_geometry(geom_);
  geom_ = pGeom;
}

void SmtGeoObject::SetGeometry(OGRGeometry* pGeom) {
  release_ogr_geometry(geom_);
  geom_ = pGeom ? pGeom->clone() : nullptr;
}

void SmtGeoObject::SetStyle(const SmtStyle* pStyle) {
  if (!pStyle) {
    return;
  }
  SMT_SAFE_DELETE(style_);
  style_ = pStyle->clone(pStyle->get_style_name());
}

SmtGeoObject* SmtGeoObject::Clone() {
  SmtGeoObject* pObj = new SmtGeoObject();
  if (!pObj) {
    return nullptr;
  }
  pObj->set_frame(frame_);
  pObj->SetGeometry(geom_);
  pObj->SetHeightSampleFn(height_fn_, height_user_);
  if (style_) {
    pObj->SetStyle(style_);
  }
  return pObj;
}

}  // namespace render
