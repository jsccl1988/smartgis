// Copyright (c) 2010 CCL. All rights reserved.
#ifndef LEGACY_RENDER_SCENE3D_INDEX_OCTREE_H
#define LEGACY_RENDER_SCENE3D_INDEX_OCTREE_H

#include "base/math/math.h"
#include "legacy/core/core.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/scene3d/scene/object.h"
#include "legacy/render/scene3d/scene/vertex3d.h"

namespace render {

// LP3DRENDERDEVICE comes from scene/object.h (forward decl only there).
class SmtFrustum;

extern int g_nSceneMaxTargets;
extern int g_nSceneMaxSubdivision;
extern int g_nSceneCurrentSubdivision;
extern int g_nSceneCurRenderTarget;
extern int g_nSceneTotalLeafNode;

class SmtSceneOctTree;

// One node in the leftover scene octree (AABB subdivision + object lists).
class LEGACY_RENDER_EXPORT SmtSceneOctTreeNode {
  friend class SmtSceneOctTree;

 public:
  SmtSceneOctTreeNode();
  ~SmtSceneOctTreeNode();

 public:
  long CreateNode(vSmt3DObjectPtrs& v3DObjectPtrs, int nTarget, Vector3 vCenter,
                  float width);

  Vector3 GetSubNodeCenter(int nSubID);

  void CreateSubNode(SmtSceneOctTreeNode* pParentNode,
                     SmtSceneOctTreeNode*& pSub,
                     vSmt3DObjectPtrs& v3DObjectPtrs, vector<bool> vbInSubNode,
                     int nTargets, int nSubID);

  void UpdateNodeObject(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  void RenderNodeObject(LP3DRENDERDEVICE p3DRenderDevice,
                        SmtFrustum& smtFrustum, bool bShowOctNodeBox = true);
  void SelectNodeObject(vSmt3DObjectPtrs& vSelected3DObjects,
                        LP3DRENDERDEVICE p3DRenderDevice,
                        SmtFrustum& smtFrustum, const lPoint& point);

  void NodeObjectModelMatrixMultiply(Matrix& matTransform);
  void NodeObjectWorldMatrixMultiply(Matrix& matTransform);

 public:
  bool IsInOctNodeAabbBox(const Vector3& point);

  SmtSceneOctTreeNode* FindMinBoxOctNode(const Ray& ray);

  SmtSceneOctTreeNode* FindMinBoxOctNode(LP3DRENDERDEVICE p3DRenderDevice,
                                         const lPoint& point);

 protected:
  SmtSceneOctTreeNode* pParentNode;
  SmtSceneOctTreeNode* pSubNodes[8];
  Vector3 vCenterPos;
  float fWidth;
  string strCode;
  bool bSubDivided;
  vSmt3DObjectPtrs v3DObjectPtrs;
  int nTargetCount;
};

class SmtScene;
struct SceneOctreeAux;

// Leftover scene spatial index: node walk for frustum render/select; unibn
// point index of object AABB centers behind SceneOctreeAux.
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
  SmtSceneOctTreeNode* m_pRootNode;
  Aabb m_aabbScene;
  SceneOctreeAux* m_aux;
  bool m_bShowNodeBox;
  int m_nAllRenderTargetsNum;
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
