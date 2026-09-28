// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MD3D_POINTCLOUD_H
#define _MD3D_POINTCLOUD_H

#include "legacy/core/core.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/3drenderdevice.h"
#include "legacy/render/rhi3d/public/resource/videobuffer.h"
#include "legacy/render/scene3d/index/vertex_octree.h"
#include "legacy/render/scene3d/scene/object.h"

using namespace render;

namespace render {
class LEGACY_RENDER_EXPORT Smt3DPointCloud : public Smt3DObject {
 public:
  Smt3DPointCloud();
  virtual ~Smt3DPointCloud();

 public:
  long Init(Vector3& vPos, SmtMaterial& matMaterial);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Destroy();

 public:
  inline bool GetShowOctNodeBox(void) { return m_bShowOctNodeBox; }
  inline void SetShowOctNodeBox(bool bShow = true) {
    m_bShowOctNodeBox = bShow;
  }

 public:
  bool Read3DPointCloud(const char* szFilePath);

  inline SmtVertexOctTree& GetVertexOctTree(void) { return m_vtxOctTree; }

 private:
  SmtVertexOctTree m_vtxOctTree;

  SmtVertexBuffer* m_pVertexBuffer;

  SmtVertex3DList m_vtxList;
  bool m_bShowOctNodeBox;

  bool m_bReadOK;
};
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_MD3D_POINTCLOUD_H