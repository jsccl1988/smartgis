// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MD3D_CUBE_H
#define _MD3D_CUBE_H

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/render/rhi3d/public/device/renderer.h"
#include "scenic/render/rhi3d/public/resource/video_buffer.h"
#include "scenic/scene3d/scene/object.h"

using namespace scenic::detail;

namespace scenic {
namespace detail {
class LEGACY_RENDER_EXPORT Cube : public Object3d {
 public:
  Cube(LP3DRENDERDEVICE pRenderDevice, ::base::Vector3 vCenter, float width);
  virtual ~Cube();

 public:
  long Init(::base::Vector3& vPos, Material& matMaterial,
            const char* szTexName = "");
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Destroy();

 private:
  VertexBuffer* m_pVertexBuffer;
  Vector3 m_vCenter;
  float m_fWidth;
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

#endif  //_MD3D_CUBE_H