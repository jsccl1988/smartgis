// Copyright (c) 2010 CCL. All rights reserved.
#ifndef LEGACY_RENDER_SCENE3D_INDEX_OCTREE_H
#define LEGACY_RENDER_SCENE3D_INDEX_OCTREE_H

#include "base/math/math.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/scene3d/scene/object.h"
#include "legacy/render/scene3d/scene/vertex3d.h"

namespace render {

// LP3DRENDERDEVICE comes from scene/object.h (forward decl only there).
class SmtFrustum;

class SmtScene;
struct SceneOctreeAux;

// Leftover scene spatial index: flat object list + unibn point index of AABB
// centers. Render/Select/Update scan the list (per-object frustum AABB cull).
class LEGACY_RENDER_EXPORT SmtSceneOctTree : public Smt3DRenderable,
                                             public Smt3DMovable {
 public:
  friend class SmtScene;

  SmtSceneOctTree();
  virtual ~SmtSceneOctTree();

 public:
  void SetShowNodeBox(bool bShowNodeBox = true) {
    m_bShowNodeBox = bShowNodeBox;
  }
  bool IsShowNodeBox(void) { return m_bShowNodeBox; }

  long CreateOctTree(vSmt3DObjectPtrs& v3DObjectPtrs);
  long DestroyTree();

 public:
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);

  void GetDebugString(char* szBuf, int nBufLength);

 public:
  void ObjectModelMatrixMultiply(Matrix& matTransform);
  void ObjectWordlMatrixMultiply(Matrix& matTransform);

  long Select3DObject(vSmt3DObjectPtrs& vSelected3DObjects,
                      LP3DRENDERDEVICE p3DRenderDevice, const lPoint& point);

 protected:
  void GetSceneDimensions(vSmt3DObjectPtrs& v3DObjectPtrs);

 protected:
  vSmt3DObjectPtrs m_objects;
  Aabb m_aabbScene;
  SceneOctreeAux* m_aux;
  bool m_bShowNodeBox;
  int m_nAllRenderTargetsNum;
  int m_nCurRenderTargets;
};
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  // LEGACY_RENDER_SCENE3D_INDEX_OCTREE_H
