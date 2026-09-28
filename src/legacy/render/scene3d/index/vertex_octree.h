// Copyright (c) 2010 CCL. All rights reserved.
#ifndef LEGACY_RENDER_SCENE3D_INDEX_VERTEX_OCTREE_H
#define LEGACY_RENDER_SCENE3D_INDEX_VERTEX_OCTREE_H

#include "base/math/math.h"
#include "legacy/core/bas_struct.h"
#include "legacy/core/core.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/scene3d/scene/vertex3d.h"

namespace render {

class Smt3DRenderDevice;
typedef Smt3DRenderDevice* LP3DRENDERDEVICE;
class SmtFrustum;
class SmtVertexBuffer;

extern int g_nMdlMaxTargets;
extern int g_nMdlMaxSubdivision;
extern int g_nMdlCurrentSubdivision;
extern int g_nMdlCurRenderTarget;
extern int g_nMdlTotalLeafNode;

class SmtVertexOctTree;

// One node in the leftover vertex octree.
class LEGACY_RENDER_EXPORT SmtVertexOctTreeNode {
  friend class SmtVertexOctTree;

 public:
  SmtVertexOctTreeNode();
  ~SmtVertexOctTreeNode();

 public:
  long CreateNode(SmtVertex3DList& lstVers, Vector3 vCenter, byte octCode,
                  double width, LP3DRENDERDEVICE p3DRenderDevice);

  Vector3 GetSubNodeCenter(int nSubID);

  uint GetSubNodeCode(int nSubID);

  void CreateSubNode(SmtVertexOctTreeNode* pParentNode,
                     SmtVertexOctTreeNode*& pSub, SmtVertex3DList& lstVers,
                     vector<bool> vbInSubNode, int nVertexs, int nSubID,
                     LP3DRENDERDEVICE p3DRenderDevice);

  void RenderNodeObject(LP3DRENDERDEVICE p3DRenderDevice,
                        SmtFrustum& smtFrustum, bool bShowOctNodeBox = true);

  int GetSubDepth();

  SmtVertexOctTreeNode* FindMinBoxOctNode(const Vector3& point);

 protected:
  SmtVertexOctTreeNode* pParentNode;
  SmtVertexOctTreeNode* pSubNodes[8];
  Vector3 vCenterPos;
  double fWidth;
  uint unOctCode;
  bool bSubDivided;
  SmtVertex3DList vertexList;
  SmtVertexBuffer* pVertexBuffer;

  bool bSelected;
};

struct VertexOctreeAux;

// Leftover vertex spatial index: node walk for render; unibn point index of
// vertex positions behind VertexOctreeAux (used by HitTestOctNode).
class LEGACY_RENDER_EXPORT SmtVertexOctTree {
 public:
  SmtVertexOctTree();
  virtual ~SmtVertexOctTree();

 public:
  long CreateOctTree(SmtVertex3DList& lstVers,
                     LP3DRENDERDEVICE p3DRenderDevice);

  void RenderTree(LP3DRENDERDEVICE p3DRenderDevice,
                  bool bShowOctNodeBox = true);

  long DestroyTree();

  inline int GetDepth(void) { return m_nDepth; }

  bool HitTestOctNode(const Vector3& point);

  void GetDebugString(char* szBuf, int nBufLength);

 protected:
  void GetSceneDimensions(SmtVertex3DList& lstVers);

 protected:
  SmtVertexOctTreeNode* m_pRootNode;
  Aabb m_aabbScene;
  VertexOctreeAux* m_aux;
  int m_nDepth;
};

}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  // LEGACY_RENDER_SCENE3D_INDEX_VERTEX_OCTREE_H
