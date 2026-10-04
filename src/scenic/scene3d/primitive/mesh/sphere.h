// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MD3D_SPHERE_H
#define _MD3D_SPHERE_H

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/render/rhi3d/public/device/renderer.h"
#include "scenic/render/rhi3d/public/resource/video_buffer.h"
#include "scenic/scene3d/scene/object.h"

using namespace scenic::detail;

namespace scenic {
namespace detail {
class LEGACY_RENDER_EXPORT Sphere : public Object3d {
 public:
  Sphere(float radius, DWORD slices);
  virtual ~Sphere();

 public:
  long Init(::base::Vector3& vPos, Material& matMaterial,
            const char* szTexName = "");
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Destroy();

  inline void SetXScale(float fScale) { m_fXScale = fScale; }
  inline void SetYScale(float fScale) { m_fYScale = fScale; }
  inline void SetZScale(float fScale) { m_fZScale = fScale; }

  inline float GetXScale(void) { return m_fXScale; }
  inline float GetYScale(void) { return m_fYScale; }
  inline float GetZScale(void) { return m_fZScale; }

 public:
  bool Select(LP3DRENDERDEVICE p3DRenderDevice, const lPoint& point);

 private:
  VertexBuffer* m_pVertexBuffer;
  float m_fRadius;
  DWORD m_dwSlices;

  float m_fZScale;
  float m_fXScale;
  float m_fYScale;
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

#endif  //_MD3D_SPHERE_H
