// Copyright (c) 2010 CCL. All rights reserved.
#ifndef SCENIC_SCENE3D_SCENE_VERTEX3D_H
#define SCENIC_SCENE3D_SCENE_VERTEX3D_H

#include <vector>

#include "base/math/math.h"
#include "scenic/render/rhi3d/public/device/base.h"

using namespace scenic::detail;
namespace scenic {
namespace detail {
struct Vertex3d {
  Vector3 ver;
  Color clr;
};

typedef std::vector<Vertex3d> vSmtVertex3Ds;

struct Vertex3dList {
  int nCount;
  Vertex3d *pVertexs;

  Vertex3dList() {
    pVertexs = NULL;
    nCount = 0;
  }

  Vertex3dList(const vSmtVertex3Ds &vOther) {
    pVertexs = NULL;
    nCount = 0;

    if (vOther.size() < 1) return;

    Release();

    pVertexs = new Vertex3d[vOther.size()];
    nCount = vOther.size();
    for (int i = 0; i < nCount; i++) {
      pVertexs[i] = vOther[i];
    }
  }

  Vertex3dList(const Vertex3dList &Other) {
    pVertexs = NULL;
    nCount = 0;

    if (this == &Other || Other.nCount < 1 || Other.pVertexs == NULL) return;

    Release();

    pVertexs = new Vertex3d[Other.nCount];
    nCount = Other.nCount;
    memcpy(pVertexs, Other.pVertexs, sizeof(Vertex3d) * Other.nCount);
  }

  Vertex3dList &operator=(const Vertex3dList &Other) {
    if (this == &Other || Other.nCount < 1 || Other.pVertexs == NULL)
      return *this;

    Release();

    pVertexs = new Vertex3d[Other.nCount];
    nCount = Other.nCount;
    memcpy(pVertexs, Other.pVertexs, sizeof(Vertex3d) * Other.nCount);

    return *this;
  }

  Vertex3dList &operator=(const vSmtVertex3Ds &vOther) {
    if (vOther.size() < 1) return *this;

    Release();

    pVertexs = new Vertex3d[vOther.size()];
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

  ~Vertex3dList() { Release(); }
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
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_SCENE_VERTEX3D_H