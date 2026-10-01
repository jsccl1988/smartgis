// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MD3D_CUBE_H
#define _MD3D_CUBE_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_device.h"
#include "legacy/render/rhi3d/public/device/renderer.h"
#include "legacy/render/rhi3d/public/resource/video_buffer.h"
#include "legacy/render/scene3d/scene/object.h"

using namespace render;

namespace render {
class LEGACY_RENDER_EXPORT SmtCube : public Smt3DObject {
 public:
  SmtCube(LP3DRENDERDEVICE pRenderDevice, ::base::Vector3 vCenter, float width);
  virtual ~SmtCube();

 public:
  long Init(::base::Vector3& vPos, SmtMaterial& matMaterial,
            const char* szTexName = "");
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Destroy();

 private:
  SmtVertexBuffer* m_pVertexBuffer;
  Vector3 m_vCenter;
  float m_fWidth;
};
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_MD3D_CUBE_H