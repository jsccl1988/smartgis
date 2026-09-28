// Copyright (c) 2010 CCL. All rights reserved.
#ifndef LEGACY_RENDER_SCENE3D_SCENE_VERTEX3D_H
#define LEGACY_RENDER_SCENE3D_SCENE_VERTEX3D_H

#include <vector>

#include "base/math/math.h"
#include "legacy/render/rhi3d/public/device/base.h"

using namespace render;
namespace render {
struct SmtVertex3D {
  Vector3 ver;
  SmtColor clr;
};

typedef std::vector<SmtVertex3D> vSmtVertex3Ds;

struct SmtVertex3DList {
  int nCount;
  SmtVertex3D *pVertexs;

  SmtVertex3DList() {
    pVertexs = NULL;
    nCount = 0;
  }

  SmtVertex3DList(const vSmtVertex3Ds &vOther) {
    pVertexs = NULL;
    nCount = 0;

    if (vOther.size() < 1) return;

    Release();

    pVertexs = new SmtVertex3D[vOther.size()];
    nCount = vOther.size();
    for (int i = 0; i < nCount; i++) {
      pVertexs[i] = vOther[i];
    }
  }

  SmtVertex3DList(const SmtVertex3DList &Other) {
    pVertexs = NULL;
    nCount = 0;

    if (this == &Other || Other.nCount < 1 || Other.pVertexs == NULL) return;

    Release();

    pVertexs = new SmtVertex3D[Other.nCount];
    nCount = Other.nCount;
    memcpy(pVertexs, Other.pVertexs, sizeof(SmtVertex3D) * Other.nCount);
  }

  SmtVertex3DList &operator=(const SmtVertex3DList &Other) {
    if (this == &Other || Other.nCount < 1 || Other.pVertexs == NULL)
      return *this;

    Release();

    pVertexs = new SmtVertex3D[Other.nCount];
    nCount = Other.nCount;
    memcpy(pVertexs, Other.pVertexs, sizeof(SmtVertex3D) * Other.nCount);

    return *this;
  }

  SmtVertex3DList &operator=(const vSmtVertex3Ds &vOther) {
    if (vOther.size() < 1) return *this;

    Release();

    pVertexs = new SmtVertex3D[vOther.size()];
    nCount = vOther.size();
    for (int i = 0; i < nCount; i++) {
      pVertexs[i] = vOther[i];
    }

    return *this;
  }

  void Release(void) {
    SMT_SAFE_DELETE_A(pVertexs);
    nCount = 0;
  }

  ~SmtVertex3DList() { Release(); }
};

enum eSmtOctreeNodes {
  TOP_LEFT_FRONT = 0,  // 0
  TOP_LEFT_BACK,       // 1
  TOP_RIGHT_BACK,      // etc...
  TOP_RIGHT_FRONT,
  BOTTOM_LEFT_FRONT,
  BOTTOM_LEFT_BACK,
  BOTTOM_RIGHT_BACK,
  BOTTOM_RIGHT_FRONT
};
}  // namespace render

#endif  // LEGACY_RENDER_SCENE3D_SCENE_VERTEX3D_H