// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MD3D_NORTHARRAY_H
#define _MD3D_NORTHARRAY_H

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/camera/camera.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/render/rhi3d/public/device/renderer.h"
#include "scenic/render/rhi3d/public/resource/video_buffer.h"
#include "scenic/scene3d/scene/object.h"

using namespace scenic::detail;

namespace scenic {
namespace detail {
class LEGACY_RENDER_EXPORT NorthArray : public Object3d {
 public:
  NorthArray(float initAngle, float fWinH, PerspCamera* pCamera);
  virtual ~NorthArray(void);

 public:
  long Init(::base::Vector3& vPos, Material& matMaterial,
            const char* szTexName = "");
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Destroy();

 public:
  void SetPerspCamera(PerspCamera* pCamera) { m_pCamera = pCamera; }

 private:
  void DrawClock(LP3DRENDERDEVICE p3DRenderDevice);
  void DrawArray(LP3DRENDERDEVICE p3DRenderDevice);

 private:
  PerspCamera* m_pCamera;
  float m_fNorthPtAngle;
  float m_fWinH;
  uint m_nFontClock;

  VertexBuffer* m_pVBClockPan;
  VertexBuffer* m_pVBClockArray;
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

#endif  //_MD3D_NORTHARRAY_H