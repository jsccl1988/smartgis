// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/index/octree.h"

#include <algorithm>
#include <vector>

#include "Octree.hpp"
#include "legacy/render/rhi3d/public/device/base.h"
#include "legacy/render/rhi3d/public/device/render_device.h"

namespace render {
namespace {

struct UnibnVec3 {
  float x;
  float y;
  float z;
};

bool object_aabb_in_frustum(SmtFrustum& frustum, Smt3DObject* obj) {
  if (!obj) {
    return false;
  }
  Vector3 max = obj->GetAabb().vcMax;
  Vector3 min = obj->GetAabb().vcMin;
  return frustum.IsBoxIn(max, min);
}

}  // namespace

// Frustum + unibn point index of object AABB centers (not exposed in header).
struct SceneOctreeAux {
  SmtFrustum frustum;
  std::vector<UnibnVec3> points;
  unibn::Octree<UnibnVec3> tree;

  void clear_points() {
    points.clear();
    tree.clear();
  }

  void rebuild_from_objects(const vSmt3DObjectPtrs& objects) {
    clear_points();
    points.reserve(objects.size());
    for (Smt3DObject* obj : objects) {
      if (!obj) {
        continue;
      }
      const Vector3& c = obj->GetAabb().vcCenter;
      points.push_back(UnibnVec3{static_cast<float>(c.x),
                                 static_cast<float>(c.y),
                                 static_cast<float>(c.z)});
    }
    if (!points.empty()) {
      tree.initialize(points);
    }
  }
};

SmtSceneOctTree::SmtSceneOctTree()
    : m_aux(new SceneOctreeAux()),
      m_nAllRenderTargetsNum(0),
      m_nCurRenderTargets(0),
      m_bShowNodeBox(true) {}

SmtSceneOctTree::~SmtSceneOctTree() {
  DestroyTree();
  delete m_aux;
  m_aux = NULL;
}

long SmtSceneOctTree::CreateOctTree(vSmt3DObjectPtrs& v3DObjectPtrs) {
  if (v3DObjectPtrs.size() < 1) {
    return SMT_ERR_INVALID_PARAM;
  }

  DestroyTree();

  m_objects = v3DObjectPtrs;
  m_nAllRenderTargetsNum = static_cast<int>(m_objects.size());

  GetSceneDimensions(m_objects);

  if (m_aux) {
    m_aux->rebuild_from_objects(m_objects);
  }

  return SMT_ERR_NONE;
}

long SmtSceneOctTree::DestroyTree() {
  m_objects.clear();
  m_nAllRenderTargetsNum = 0;
  m_nCurRenderTargets = 0;
  m_aabbScene = Aabb();
  if (m_aux) {
    m_aux->clear_points();
  }
  return SMT_ERR_NONE;
}

void SmtSceneOctTree::GetSceneDimensions(vSmt3DObjectPtrs& v3DObjectPtrs) {
  for (Smt3DObject* obj : v3DObjectPtrs) {
    if (obj) {
      m_aabbScene.merge(obj->GetAabb());
    }
  }
  m_aabbScene.vcCenter = (m_aabbScene.vcMax + m_aabbScene.vcMin) / 2.;
}

long SmtSceneOctTree::Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) {
  m_nCurRenderTargets = 0;
  for (Smt3DObject* obj : m_objects) {
    if (obj) {
      obj->Update(p3DRenderDevice, fElapsed);
      ++m_nCurRenderTargets;
    }
  }
  return SMT_ERR_NONE;
}

long SmtSceneOctTree::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (!m_aux || m_objects.empty()) {
    return SMT_ERR_NONE;
  }

  m_nCurRenderTargets = 0;
  p3DRenderDevice->GetFrustum(m_aux->frustum);

  if (m_bShowNodeBox) {
    SmtGPUStateManager* stateManager = p3DRenderDevice->GetStateManager();
    stateManager->SetLight(false);
    stateManager->Set2DTextures(false);
    const float width = static_cast<float>(
        (std::max)(m_aabbScene.vcMax.x - m_aabbScene.vcMin.x,
                   (std::max)(m_aabbScene.vcMax.y - m_aabbScene.vcMin.y,
                              m_aabbScene.vcMax.z - m_aabbScene.vcMin.z)));
    p3DRenderDevice->DrawCube3D(m_aabbScene.vcCenter, width,
                                SmtColor(0., 1., 0., 1.));
    stateManager->SetLight(true);
    stateManager->Set2DTextures(true);
  }

  for (Smt3DObject* obj : m_objects) {
    if (!obj || !obj->IsVisible()) {
      continue;
    }
    if (!object_aabb_in_frustum(m_aux->frustum, obj)) {
      continue;
    }
    obj->Render(p3DRenderDevice);
    ++m_nCurRenderTargets;
  }
  return SMT_ERR_NONE;
}

long SmtSceneOctTree::Select3DObject(vSmt3DObjectPtrs& vSelected3DObjects,
                                     LP3DRENDERDEVICE p3DRenderDevice,
                                     const lPoint& point) {
  if (!m_aux || m_objects.empty()) {
    return SMT_ERR_FAILURE;
  }

  m_nCurRenderTargets = 0;
  p3DRenderDevice->GetFrustum(m_aux->frustum);

  for (Smt3DObject* obj : m_objects) {
    if (!obj) {
      continue;
    }
    if (!object_aabb_in_frustum(m_aux->frustum, obj)) {
      continue;
    }
    if (obj->Select(p3DRenderDevice, point)) {
      vSelected3DObjects.push_back(obj);
    }
  }

  return SMT_ERR_FAILURE;
}

void SmtSceneOctTree::ObjectModelMatrixMultiply(Matrix& matTransform) {
  for (Smt3DObject* obj : m_objects) {
    if (obj && obj->IsVisible()) {
      obj->ModelTransMatrixMultiply(matTransform);
    }
  }
}

void SmtSceneOctTree::ObjectWordlMatrixMultiply(Matrix& matTransform) {
  for (Smt3DObject* obj : m_objects) {
    if (obj && obj->IsVisible()) {
      obj->WorldTransMatrixMultiply(matTransform);
    }
  }
}

void SmtSceneOctTree::GetDebugString(char* szBuf, int nBufLength) {
  const size_t indexed = m_aux ? m_aux->points.size() : 0;
  snprintf(szBuf, nBufLength, "render target:%d/%d;unibn points:%zu",
           m_nCurRenderTargets, m_nAllRenderTargetsNum, indexed);
}

}  // namespace render
