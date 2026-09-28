// Copyright (c) 2010 CCL. All rights reserved.
#ifndef LEGACY_RENDER_SCENE3D_SCENE_SCENE_H
#define LEGACY_RENDER_SCENE3D_SCENE_SCENE_H

#include <mutex>

#include "base/time/frame_timer.h"
#include "legacy/core/core.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/camera/camera.h"
#include "legacy/render/rhi3d/public/device/3drenderdevice.h"
#include "legacy/render/rhi3d/public/device/base.h"
#include "legacy/render/scene3d/index/octree.h"
#include "legacy/render/scene3d/primitive/northarray.h"
#include "legacy/render/scene3d/scene/object.h"

using namespace base;
using namespace render;
using namespace render;
using namespace render;

namespace render {
class LEGACY_RENDER_EXPORT SmtScene {
 public:
  SmtScene(void);
  virtual ~SmtScene(void);

 public:
  inline LP3DRENDERDEVICE Get3DRenderDevice() { return m_p3DRenderDevice; }
  inline void Set3DRenderDevice(LP3DRENDERDEVICE p3DRenderDevice) {
    m_p3DRenderDevice = p3DRenderDevice;
  }

  inline SmtPerspCamera *GetSceneCamera() { return m_pCamera; }
  inline void SetSceneCamera(SmtPerspCamera *pCamera);

  inline Aabb &GetAabb() { return m_aAbb; }
  inline void SetAabb(Aabb &aabb) { m_aAbb = aabb; }

 public:
  long Setup(void);
  long Update(void);
  long Render(void);

 public:
  long Transform2DTo3D(Vector3 &vOrg, Vector3 &vTar, const lPoint &point);
  long Transform3DTo2D(const Vector3 &ver3D, lPoint &point);

 public:
  void Add3DObject(Smt3DObject *p3DObject);
  Smt3DObject *Get3DObject(int index);
  const Smt3DObject *Get3DObject(int index) const;
  void Remove3DObject(Smt3DObject *p3DObject);
  void Remove3DObject(int index);
  void Get3DObjectPtrs(vSmt3DObjectPtrs &v3DObjectPtrs);

  void CreateOctTreeSceneMgr(void);

 public:
  void SetShowNodeBox(bool bShowNodeBox = true) {
    m_bShowNodeBox = bShowNodeBox;
  }
  bool IsShowNodeBox(void) { return m_bShowNodeBox; }

 public:
  long TransModel3DObjects(Matrix &matTransform);

  long TransWorld3DObjects(Matrix &matTransform);

  // ʰȡ
  long Select3DObject(vSmt3DObjectPtrs &vSelected3DObjects, lPoint point);

 protected:
  bool Update3DObjCatalog(void);

 private:
  LP3DRENDERDEVICE m_p3DRenderDevice;

  SmtSceneOctTree *m_pSceneTree;
  bool m_bOctTreeCreated;
  bool m_bShowNodeBox;
  vSmt3DObjectPtrs m_v3DObjectPtrs;

  ::base::FrameTimer *m_pTimer;
  SmtPerspCamera *m_pCamera;
  SmtNorthArray *m_pNorthArray;

  Vector3 m_vOrgPos;
  Aabb m_aAbb;

  uint m_nHelpInfoFont;
  uint m_nRenderInfoFont;
  uint m_nTimerInfoFont;

  char m_szHelpInfoBuf[TEMP_BUFFER_SIZE];
  char m_szRenderInfoBuf[TEMP_BUFFER_SIZE];
};
}  // namespace render
#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  // LEGACY_RENDER_SCENE3D_SCENE_SCENE_H