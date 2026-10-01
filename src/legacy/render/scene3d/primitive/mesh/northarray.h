// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MD3D_NORTHARRAY_H
#define _MD3D_NORTHARRAY_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/camera/camera.h"
#include "legacy/render/rhi3d/public/device/render_device.h"
#include "legacy/render/rhi3d/public/device/renderer.h"
#include "legacy/render/rhi3d/public/resource/video_buffer.h"
#include "legacy/render/scene3d/scene/object.h"

using namespace render;

namespace render {
class LEGACY_RENDER_EXPORT SmtNorthArray : public Smt3DObject {
 public:
  SmtNorthArray(float initAngle, float fWinH, SmtPerspCamera* pCamera);
  virtual ~SmtNorthArray(void);

 public:
  long Init(::base::Vector3& vPos, SmtMaterial& matMaterial,
            const char* szTexName = "");
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Destroy();

 public:
  void SetPerspCamera(SmtPerspCamera* pCamera) { m_pCamera = pCamera; }

 private:
  void DrawClock(LP3DRENDERDEVICE p3DRenderDevice);
  void DrawArray(LP3DRENDERDEVICE p3DRenderDevice);

 private:
  SmtPerspCamera* m_pCamera;
  float m_fNorthPtAngle;
  float m_fWinH;
  uint m_nFontClock;

  SmtVertexBuffer* m_pVBClockPan;
  SmtVertexBuffer* m_pVBClockArray;
};
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_MD3D_NORTHARRAY_H