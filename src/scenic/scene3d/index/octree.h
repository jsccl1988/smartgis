// Copyright (c) 2010 CCL. All rights reserved.
#ifndef SCENIC_SCENE3D_INDEX_OCTREE_H
#define SCENIC_SCENE3D_INDEX_OCTREE_H

#include "base/math/math.h"
#include "scenic/detail/err.h"
#include "scenic/scenic_impl_export.h"
#include "scenic/scene3d/scene/object.h"
#include "scenic/scene3d/scene/vertex3d.h"

namespace scenic {
namespace detail {

// LP3DRENDERDEVICE comes from scene/object.h (forward decl only there).

class Scene;
struct SceneOctreeAux;

// Leftover scene spatial index: flat object list + unibn point index of AABB
// centers. Render/Select/Update scan the list (per-object frustum AABB cull).
class LEGACY_RENDER_EXPORT SceneOctTree : public Renderable3d,
                                             public Movable3d {
 public:
  friend class Scene;

  SceneOctTree();
  virtual ~SceneOctTree();

 public:
  void SetShowNodeBox(bool bShowNodeBox = true) {
    m_bShowNodeBox = bShowNodeBox;
  }
  bool IsShowNodeBox(void) { return m_bShowNodeBox; }

  long CreateOctTree(Object3dPtrs& v3DObjectPtrs);
  long DestroyTree();

 public:
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);

  void GetDebugString(char* szBuf, int nBufLength);

 public:
  void ObjectModelMatrixMultiply(Matrix& matTransform);
  void ObjectWordlMatrixMultiply(Matrix& matTransform);

  long Select3DObject(Object3dPtrs& vSelected3DObjects,
                      LP3DRENDERDEVICE p3DRenderDevice, const lPoint& point);

 protected:
  void GetSceneDimensions(Object3dPtrs& v3DObjectPtrs);

 protected:
  Object3dPtrs m_objects;
  Aabb m_aabbScene;
  SceneOctreeAux* m_aux;
  bool m_bShowNodeBox;
  int m_nAllRenderTargetsNum;
  int m_nCurRenderTargets;
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

#endif  // SCENIC_SCENE3D_INDEX_OCTREE_H
