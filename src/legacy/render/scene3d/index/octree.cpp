// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/index/octree.h"

#include <algorithm>
#include <vector>

#include "Octree.hpp"
#include "legacy/render/rhi3d/public/device/3drenderdevice.h"
#include "legacy/render/rhi3d/public/device/base.h"

namespace render {
namespace {

struct UnibnVec3 {
  float x;
  float y;
  float z;
};

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

int g_nSceneMaxTargets = 40;
int g_nSceneMaxSubdivision = 6;
int g_nSceneCurrentSubdivision = 0;
int g_nSceneCurRenderTarget = 0;
int g_nSceneTotalLeafNode = 0;

SmtSceneOctTree::SmtSceneOctTree()
    : m_pRootNode(NULL),
      m_aux(new SceneOctreeAux()),
      m_nAllRenderTargetsNum(0),
      m_bShowNodeBox(true) {}

SmtSceneOctTree::~SmtSceneOctTree() {
  DestroyTree();
  delete m_aux;
  m_aux = NULL;
}

long SmtSceneOctTree::CreateOctTree(vSmt3DObjectPtrs& v3DObjectPtrs) {
  if (v3DObjectPtrs.size() < 1) return SMT_ERR_INVALID_PARAM;

  DestroyTree();

  m_nAllRenderTargetsNum = static_cast<int>(v3DObjectPtrs.size());

  m_pRootNode = new SmtSceneOctTreeNode();

  GetSceneDimensions(v3DObjectPtrs);

  const float extent_xy =
      static_cast<float>((std::max)(m_aabbScene.vcMax.x - m_aabbScene.vcMin.x,
                                    m_aabbScene.vcMax.y - m_aabbScene.vcMin.y));
  const float extent_z =
      static_cast<float>(m_aabbScene.vcMax.z - m_aabbScene.vcMin.z);
  m_pRootNode->fWidth = (std::max)(extent_xy, extent_z);

  m_aabbScene.merge(m_aabbScene.vcCenter - m_pRootNode->fWidth / 2);
  m_aabbScene.merge(m_aabbScene.vcCenter + m_pRootNode->fWidth / 2);

  m_pRootNode->vCenterPos = m_aabbScene.vcCenter;

  m_pRootNode->CreateNode(v3DObjectPtrs, static_cast<int>(v3DObjectPtrs.size()),
                          m_pRootNode->vCenterPos, m_pRootNode->fWidth);

  if (m_aux) {
    m_aux->rebuild_from_objects(v3DObjectPtrs);
  }

  return SMT_ERR_NONE;
}

long SmtSceneOctTree::DestroyTree() {
  SMT_SAFE_DELETE(m_pRootNode);

  g_nSceneCurrentSubdivision = 0;
  m_nAllRenderTargetsNum = 0;

  m_aabbScene = Aabb();
  if (m_aux) {
    m_aux->clear_points();
  }

  return SMT_ERR_NONE;
}

void SmtSceneOctTree::GetSceneDimensions(vSmt3DObjectPtrs& v3DObjectPtrs) {
  vSmt3DObjectPtrs::iterator iter = v3DObjectPtrs.begin();
  while (iter != v3DObjectPtrs.end()) {
    m_aabbScene.merge((*iter)->GetAabb());
    iter++;
  }

  m_aabbScene.vcCenter = (m_aabbScene.vcMax + m_aabbScene.vcMin) / 2.;
}

long SmtSceneOctTree::Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) {
  if (NULL != m_pRootNode) {
    m_pRootNode->UpdateNodeObject(p3DRenderDevice, fElapsed);
  }
  return SMT_ERR_NONE;
}

long SmtSceneOctTree::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (NULL != m_pRootNode && m_aux) {
    g_nSceneCurRenderTarget = 0;
    p3DRenderDevice->GetFrustum(m_aux->frustum);
    m_pRootNode->RenderNodeObject(p3DRenderDevice, m_aux->frustum,
                                  m_bShowNodeBox);
  }
  return SMT_ERR_NONE;
}

long SmtSceneOctTree::Select3DObject(vSmt3DObjectPtrs& vSelected3DObjects,
                                     LP3DRENDERDEVICE p3DRenderDevice,
                                     const lPoint& point) {
  if (NULL != m_pRootNode && m_aux) {
    g_nSceneCurRenderTarget = 0;
    p3DRenderDevice->GetFrustum(m_aux->frustum);
    m_pRootNode->SelectNodeObject(vSelected3DObjects, p3DRenderDevice,
                                  m_aux->frustum, point);
  }

  return SMT_ERR_FAILURE;
}

void SmtSceneOctTree::ObjectModelMatrixMultiply(Matrix& matTransform) {
  if (NULL != m_pRootNode) {
    m_pRootNode->NodeObjectModelMatrixMultiply(matTransform);
  }
}

void SmtSceneOctTree::ObjectWordlMatrixMultiply(Matrix& matTransform) {
  if (NULL != m_pRootNode) {
    m_pRootNode->NodeObjectWorldMatrixMultiply(matTransform);
  }
}

void SmtSceneOctTree::GetDebugString(char* szBuf, int nBufLength) {
  snprintf(szBuf, nBufLength, "render target:%d/%d;subdivision:%d,-leaf:%d",
           g_nSceneCurRenderTarget, m_nAllRenderTargetsNum,
           g_nSceneCurrentSubdivision, g_nSceneTotalLeafNode);
}

}  // namespace render
