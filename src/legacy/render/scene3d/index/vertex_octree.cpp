// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/index/vertex_octree.h"

#include <vector>

#include "Octree.hpp"

namespace render {
namespace {

struct UnibnVec3 {
  float x;
  float y;
  float z;
};

UnibnVec3 to_unibn(const Vector3& v) {
  return UnibnVec3{static_cast<float>(v.x), static_cast<float>(v.y),
                   static_cast<float>(v.z)};
}

}  // namespace

// unibn point index of vertex positions (not exposed in header).
struct VertexOctreeAux {
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
      points.push_back(to_unibn(lstVers.pVertexs[i].ver));
    }
    if (!points.empty()) {
      tree.initialize(points);
    }
  }
};

SmtVertexOctTree::SmtVertexOctTree()
    : m_aux(new VertexOctreeAux()), m_nDepth(0), m_nVertexCount(0) {}

SmtVertexOctTree::~SmtVertexOctTree() {
  DestroyTree();
  delete m_aux;
  m_aux = NULL;
}

long SmtVertexOctTree::build(const SmtVertex3DList& lstVers) {
  if (lstVers.nCount < 1 || !lstVers.pVertexs) {
    return SMT_ERR_INVALID_PARAM;
  }

  DestroyTree();
  GetSceneDimensions(lstVers);
  m_nVertexCount = lstVers.nCount;
  m_nDepth = 1;
  if (m_aux) {
    m_aux->rebuild_from_vertices(lstVers);
  }
  return SMT_ERR_NONE;
}

long SmtVertexOctTree::CreateOctTree(SmtVertex3DList& lstVers) {
  return build(lstVers);
}

long SmtVertexOctTree::DestroyTree() {
  m_aabbScene = Aabb();
  m_nDepth = 0;
  m_nVertexCount = 0;
  if (m_aux) {
    m_aux->clear_points();
  }
  return SMT_ERR_NONE;
}

void SmtVertexOctTree::GetSceneDimensions(const SmtVertex3DList& lstVers) {
  for (int i = 0; i < lstVers.nCount; ++i) {
    m_aabbScene.merge(lstVers.pVertexs[i].ver);
  }
  m_aabbScene.vcCenter = (m_aabbScene.vcMax + m_aabbScene.vcMin) / 2.;
}

bool SmtVertexOctTree::hit_test(const Vector3& point, float radius) const {
  if (!m_aux || m_aux->points.empty() || radius < 0.0f) {
    return false;
  }
  std::vector<uint32_t> hits;
  radius_neighbors(point, radius, &hits);
  return !hits.empty();
}

int SmtVertexOctTree::find_nearest(const Vector3& point,
                                   float min_distance) const {
  if (!m_aux || m_aux->points.empty()) {
    return -1;
  }
  const UnibnVec3 q = to_unibn(point);
  return static_cast<int>(
      m_aux->tree.findNeighbor<unibn::L2Distance<UnibnVec3>>(q, min_distance));
}

void SmtVertexOctTree::radius_neighbors(const Vector3& point, float radius,
                                        std::vector<uint32_t>* out) const {
  if (!out) {
    return;
  }
  out->clear();
  if (!m_aux || m_aux->points.empty() || radius < 0.0f) {
    return;
  }
  const UnibnVec3 q = to_unibn(point);
  m_aux->tree.radiusNeighbors<unibn::L2Distance<UnibnVec3>>(q, radius, *out);
}

bool SmtVertexOctTree::HitTestOctNode(const Vector3& point) {
  return hit_test(point, 1.0e-3f);
}

void SmtVertexOctTree::GetDebugString(char* szBuf, int nBufLength) const {
  const size_t indexed = m_aux ? m_aux->points.size() : 0;
  snprintf(szBuf, nBufLength, "point index unibn:%zu depth:%d", indexed,
           m_nDepth);
}

}  // namespace render
