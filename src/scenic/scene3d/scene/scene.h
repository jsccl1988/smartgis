// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_SCENE_SCENE_H
#define SCENIC_SCENE3D_SCENE_SCENE_H

#include <mutex>

#include "base/time/frame_timer.h"
#include "scenic/render/err.h"
#include "base/math/math.h"
#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/camera/camera.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/render/rhi3d/public/device/base.h"
#include "scenic/scene3d/primitive/mesh/northarray.h"
#include "scenic/scene3d/scene/object.h"

using namespace base;
using namespace scenic::detail;
using namespace scenic::detail;
using namespace scenic::detail;

namespace scenic {
namespace detail {

class SceneOctree;

// Leftover 3D scene graph: object list, optional flat octree, camera, HUD.
class SCENIC_IMPL_EXPORT Scene {
 public:
  Scene(void);
  virtual ~Scene(void);

 public:
  inline LP3DRENDERDEVICE Get3DRenderDevice() { return m_p3DRenderDevice; }
  inline void Set3DRenderDevice(LP3DRENDERDEVICE p3DRenderDevice) {
    m_p3DRenderDevice = p3DRenderDevice;
  }

  inline PerspCamera *GetSceneCamera() { return m_pCamera; }
  inline void SetSceneCamera(PerspCamera *pCamera);

  inline ::base::Aabb &GetAabb() { return m_aAbb; }
  inline void SetAabb(::base::Aabb &aabb) { m_aAbb = aabb; }

 public:
  long Setup(void);
  long Update(void);
  long Render(void);

 public:
  long Transform2DTo3D(::base::Vector3 &vOrg, ::base::Vector3 &vTar,
                       const lPoint &point);
  long Transform3DTo2D(const ::base::Vector3 &ver3D, lPoint &point);

 public:
  void Add3DObject(Object3d *p3DObject);
  Object3d *Get3DObject(int index);
  const Object3d *Get3DObject(int index) const;
  void Remove3DObject(Object3d *p3DObject);
  void Remove3DObject(int index);
  void Get3DObjectPtrs(Object3dPtrs &v3DObjectPtrs);

  void CreateOctTreeSceneMgr(void);

 public:
  void SetShowNodeBox(bool bShowNodeBox = true) {
    m_bShowNodeBox = bShowNodeBox;
  }
  bool IsShowNodeBox(void) { return m_bShowNodeBox; }

 public:
  long TransModel3DObjects(::base::Matrix &matTransform);

  long TransWorld3DObjects(::base::Matrix &matTransform);

  // ʰȡ
  long Select3DObject(Object3dPtrs &vSelected3DObjects, lPoint point);

 protected:
  bool Update3DObjCatalog(void);

 private:
  LP3DRENDERDEVICE m_p3DRenderDevice;

  SceneOctree *m_pSceneTree;
  bool m_bOctTreeCreated;
  bool m_bShowNodeBox;
  Object3dPtrs m_v3DObjectPtrs;

  ::base::FrameTimer *m_pTimer;
  PerspCamera *m_pCamera;
  NorthArray *m_pNorthArray;

  Vector3 m_vOrgPos;
  Aabb m_aAbb;

  uint m_nHelpInfoFont;
  uint m_nRenderInfoFont;
  uint m_nTimerInfoFont;

  char m_szHelpInfoBuf[TEMP_BUFFER_SIZE];
  char m_szRenderInfoBuf[TEMP_BUFFER_SIZE];
};
}  // namespace detail
}  // namespace scenic
#if !defined(SCENIC_IMPL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  // SCENIC_SCENE3D_SCENE_SCENE_H