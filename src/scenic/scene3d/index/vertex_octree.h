// Copyright (c) 2010 CCL. All rights reserved.
#ifndef SCENIC_SCENE3D_INDEX_VERTEX_OCTREE_H
#define SCENIC_SCENE3D_INDEX_VERTEX_OCTREE_H

#include <cstdint>
#include <vector>

#include "base/math/math.h"
#include "scenic/detail/err.h"
#include "scenic/detail/geom.h"
#include "scenic/scenic_impl_export.h"
#include "scenic/scene3d/scene/vertex3d.h"

namespace scenic {
namespace detail {

struct VertexOctreeAux;

// Point-cloud spatial index (unibn). Query only — no VB / draw ownership.
// Render path lives on PointCloud3d (flat or chunked VB).
class LEGACY_RENDER_EXPORT VertexOctTree {
 public:
  VertexOctTree();
  virtual ~VertexOctTree();

  // Build / clear the unibn index from CPU vertex positions.
  long build(const Vertex3dList& lstVers);
  long DestroyTree();

  // Legacy alias for build().
  long CreateOctTree(Vertex3dList& lstVers);

  // True if any point lies within radius of `point` (L2).
  bool hit_test(const Vector3& point, float radius = 1.0e-3f) const;

  // Index of nearest neighbor, or -1 if empty. min_distance < 0 disables
  // the unibn "keep away" filter.
  int find_nearest(const Vector3& point, float min_distance = -1.0f) const;

  // Append indices of all points within L2 radius into *out (cleared first).
  void radius_neighbors(const Vector3& point, float radius,
                        std::vector<uint32_t>* out) const;

  // Leftover ABI: hit_test with default radius.
  bool HitTestOctNode(const Vector3& point);

  inline int GetDepth(void) const { return m_nDepth; }
  inline int vertex_count() const { return m_nVertexCount; }
  inline const Aabb& aabb() const { return m_aabbScene; }
  inline bool empty() const { return m_nVertexCount < 1; }

  void GetDebugString(char* szBuf, int nBufLength) const;

 protected:
  void GetSceneDimensions(const Vertex3dList& lstVers);

 protected:
  Aabb m_aabbScene;
  VertexOctreeAux* m_aux;
  int m_nDepth;
  int m_nVertexCount;
};

}  // namespace detail
}  // namespace scenic

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  // SCENIC_SCENE3D_INDEX_VERTEX_OCTREE_H
