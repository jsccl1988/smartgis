// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/index/vertex_octree.h"

#include <algorithm>
#include <vector>

#include "Octree.hpp"
#include "legacy/core/bas_struct.h"
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

// Frustum + unibn point index of vertex positions.
struct VertexOctreeAux {
  SmtFrustum frustum;
  std::vector<UnibnVec3> points;
  unibn::Octree<UnibnVec3> tree;

  void clear_points() {
    points.clear();
    tree.clear();
  }

  void rebuild_from_vertices(const SmtVertex3DList& lstVers) {
    clear_points();
    if (lstVers.nCount < 1 || !lstVers.pVertexs) {
      return;
    }
    points.reserve(static_cast<size_t>(lstVers.nCount));
    for (int i = 0; i < lstVers.nCount; ++i) {
      const Vector3& v = lstVers.pVertexs[i].ver;
      points.push_back(UnibnVec3{static_cast<float>(v.x),
                                 static_cast<float>(v.y),
                                 static_cast<float>(v.z)});
    }
    if (!points.empty()) {
      tree.initialize(points);
    }
  }

  bool has_neighbor_near(const Vector3& point, float radius) const {
    if (points.empty()) {
      return false;
    }
    UnibnVec3 q{static_cast<float>(point.x), static_cast<float>(point.y),
                static_cast<float>(point.z)};
    std::vector<uint32_t> hits;
    tree.radiusNeighbors<unibn::L2Distance<UnibnVec3>>(q, radius, hits);
    return !hits.empty();
  }
};

int g_nMdlMaxTargets = 100;
int g_nMdlMaxSubdivision = 5;
int g_nMdlCurrentSubdivision = 0;
int g_nMdlCurRenderTarget = 0;
int g_nMdlTotalLeafNode = 0;

SmtVertexOctTree::SmtVertexOctTree()
    : m_pRootNode(NULL), m_aux(new VertexOctreeAux()), m_nDepth(0) {}

SmtVertexOctTree::~SmtVertexOctTree() {
  DestroyTree();
  delete m_aux;
  m_aux = NULL;
}

long SmtVertexOctTree::CreateOctTree(SmtVertex3DList& lstVers,
                                     LP3DRENDERDEVICE p3DRenderDevice) {
  if (lstVers.nCount < 1) return SMT_ERR_INVALID_PARAM;

  DestroyTree();

  m_pRootNode = new SmtVertexOctTreeNode();

  GetSceneDimensions(lstVers);

  const float extent_xy =
      static_cast<float>((std::max)(m_aabbScene.vcMax.x - m_aabbScene.vcMin.x,
                                    m_aabbScene.vcMax.y - m_aabbScene.vcMin.y));
  const float extent_z =
      static_cast<float>(m_aabbScene.vcMax.z - m_aabbScene.vcMin.z);
  m_pRootNode->fWidth = (std::max)(extent_xy, extent_z);

  m_aabbScene.merge(m_aabbScene.vcCenter - m_pRootNode->fWidth / 2);
  m_aabbScene.merge(m_aabbScene.vcCenter + m_pRootNode->fWidth / 2);

  m_pRootNode->vCenterPos = m_aabbScene.vcCenter;
  m_pRootNode->unOctCode = 0;

  m_pRootNode->CreateNode(lstVers, m_pRootNode->vCenterPos,
                          m_pRootNode->unOctCode, m_pRootNode->fWidth,
                          p3DRenderDevice);

  m_nDepth = m_pRootNode->GetSubDepth();

  if (m_aux) {
    m_aux->rebuild_from_vertices(lstVers);
  }

  return SMT_ERR_NONE;
}

long SmtVertexOctTree::DestroyTree() {
  SMT_SAFE_DELETE(m_pRootNode);

  g_nMdlCurrentSubdivision = 0;
  m_aabbScene = Aabb();
  if (m_aux) {
    m_aux->clear_points();
  }

  return SMT_ERR_NONE;
}

void SmtVertexOctTree::GetSceneDimensions(SmtVertex3DList& lstVers) {
  for (int i = 0; i < lstVers.nCount; i++) {
    Vector3 vPos = lstVers.pVertexs[i].ver;
    m_aabbScene.merge(vPos);
  }

  m_aabbScene.vcCenter = (m_aabbScene.vcMax + m_aabbScene.vcMin) / 2.;
}

void SmtVertexOctTree::RenderTree(LP3DRENDERDEVICE p3DRenderDevice,
                                  bool bShowOctNodeBox) {
  static char szBuf[TEMP_BUFFER_SIZE];
  if (NULL != m_pRootNode && m_aux) {
    g_nMdlCurRenderTarget = 0;
    p3DRenderDevice->GetFrustum(m_aux->frustum);
    m_pRootNode->RenderNodeObject(p3DRenderDevice, m_aux->frustum,
                                  bShowOctNodeBox);
    GetDebugString(szBuf, TEMP_BUFFER_SIZE);
    p3DRenderDevice->DrawText(0, 10, 100, SmtColor(0., 1., 1.), szBuf);
  }
}

bool SmtVertexOctTree::HitTestOctNode(const Vector3& point) {
  if (!m_pRootNode) {
    return false;
  }

  // Prefer unibn radius query when the point index is populated.
  if (m_aux && m_aux->has_neighbor_near(point, 1.0e-3f)) {
    SmtVertexOctTreeNode* pNode = m_pRootNode->FindMinBoxOctNode(point);
    if (pNode) {
      pNode->bSelected = !pNode->bSelected;
      return true;
    }
  }

  SmtVertexOctTreeNode* pNode = m_pRootNode->FindMinBoxOctNode(point);
  if (pNode) {
    pNode->bSelected = !pNode->bSelected;
    return true;
  }
  return false;
}

void SmtVertexOctTree::GetDebugString(char* szBuf, int nBufLength) {
  snprintf(szBuf, nBufLength,
           " Render Point:%d;SmtVertexOctTree-Depth:%d,-TotalLeafNode:%d",
           g_nMdlCurRenderTarget, m_nDepth, g_nMdlTotalLeafNode);
}

}  // namespace render
