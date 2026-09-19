// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/stereo_terrain.h"

#include <vector>

#include "legacy/render/render3d/statesmanager.h"

namespace render {

StereoTerrain::StereoTerrain() = default;

StereoTerrain::~StereoTerrain() { Destroy(); }

void StereoTerrain::set_height_field(const DemHeightField* field) {
  owned_field_.reset();
  field_ = field;
}

void StereoTerrain::adopt_height_field(DemHeightField* field) {
  owned_field_.reset(field);
  field_ = owned_field_.get();
}

long StereoTerrain::Init(Vector3& vPos, SmtMaterial& matMaterial,
                         const char* szTexName) {
  return Smt3DObject::Init(vPos, matMaterial, szTexName);
}

long StereoTerrain::Create(LP3DRENDERDEVICE p3DRenderDevice) {
  Destroy();
  if (!field_ || field_->empty()) {
    return SMT_ERR_INVALID_PARAM;
  }
  // Null device: keep owned height field attached (tests / deferred GL upload).
  if (!p3DRenderDevice) {
    return SMT_ERR_NONE;
  }
  std::vector<float> xyz;
  std::vector<unsigned> indices;
  std::vector<float> rgb;
  std::vector<float> nrm;
  if (!field_->build_mesh(192, &xyz, &indices, &rgb, &nrm) || xyz.empty() ||
      indices.empty()) {
    return SMT_ERR_FAILURE;
  }
  const int nvert = static_cast<int>(xyz.size() / 3);
  vb_ = p3DRenderDevice->CreateVertexBuffer(
      nvert, VF_XYZ | VF_NORMAL | VF_DIFFUSE, false);
  if (!vb_) {
    return SMT_ERR_FAILURE;
  }
  vb_->Lock();
  for (int i = 0; i < nvert; ++i) {
    const size_t o = static_cast<size_t>(i) * 3;
    vb_->Normal(nrm[o], nrm[o + 1], nrm[o + 2]);
    vb_->Diffuse(rgb[o], rgb[o + 1], rgb[o + 2], 1.f);
    vb_->Vertex(xyz[o], xyz[o + 1], xyz[o + 2]);
    m_aAbb.merge(xyz[o], xyz[o + 1], xyz[o + 2]);
  }
  vb_->Unlock();
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.f;

  ib_ = p3DRenderDevice->CreateIndexBuffer(static_cast<int>(indices.size()));
  if (!ib_) {
    SMT_SAFE_DELETE(vb_);
    return SMT_ERR_FAILURE;
  }
  ib_->Lock();
  for (unsigned ix : indices) {
    ib_->Index(static_cast<int>(ix));
  }
  ib_->Unlock();
  index_count_ = static_cast<ulong>(indices.size());
  return SMT_ERR_NONE;
}

long StereoTerrain::Update(LP3DRENDERDEVICE, float) { return SMT_ERR_NONE; }

long StereoTerrain::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (!p3DRenderDevice || !vb_ || !ib_ || index_count_ < 3) {
    return SMT_ERR_INVALID_PARAM;
  }
  p3DRenderDevice->SetBackfaceCulling(RSV_CULL_NONE);
  p3DRenderDevice->SetShadeMode(RSV_SHADE_SOLID, 0,
                                SmtColor(1.f, 1.f, 1.f, 1.f));
  if (SmtGPUStateManager* states = p3DRenderDevice->GetStateManager()) {
    states->SetLight(true);
    states->Set2DTextures(false);
    // Push terrain slightly into the depth buffer so draped vectors win cleanly.
    states->DepthOffsetParams(1.0f, 2.0f);
    states->EnableDepthOffset(PM_FILL, true);
  }
  p3DRenderDevice->SetMaterial(&m_matMaterial);
  p3DRenderDevice->DrawIndexedPrimitives(PT_TRIANGLELIST, vb_, ib_, 0,
                                         index_count_ / 3);
  if (SmtGPUStateManager* states = p3DRenderDevice->GetStateManager()) {
    states->EnableDepthOffset(PM_FILL, false);
    states->DepthOffsetParams(0.f, 0.f);
  }
  return SMT_ERR_NONE;
}

long StereoTerrain::Destroy() {
  SMT_SAFE_DELETE(vb_);
  SMT_SAFE_DELETE(ib_);
  index_count_ = 0;
  return SMT_ERR_NONE;
}

}  // namespace render
