// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_RENDERBUFFER_H
#define _RD3D_RENDERBUFFER_H

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/base.h"
#include "scenic/render/rhi3d/public/texture/texture.h"

namespace scenic {
namespace detail {
class RenderDevice3d;
typedef class RenderDevice3d *LP3DRENDERDEVICE;

class LEGACY_RENDER_EXPORT RenderBuffer {
 public:
  RenderBuffer(LP3DRENDERDEVICE p3DRenderDevice, uint handle,
                  TextureFormat format, uint width, uint height)
      : m_p3DRenderDevice(p3DRenderDevice),
        m_unHandle(height),
        m_texFormat(format),
        m_unWidth(width),
        m_unHeight(height) {
    ;
  }

  virtual ~RenderBuffer() {}

 public:
  inline uint GetHandle(void) const { return m_unHandle; }

  inline uint GetWidth(void) const { return m_unWidth; }
  inline uint GetHeight(void) const { return m_unHeight; }
  inline TextureFormat GetFormat(void) const { return m_texFormat; }

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  uint m_unHandle;

  uint m_unWidth;
  uint m_unHeight;
  TextureFormat m_texFormat;
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

#endif  //_RD3D_RENDERBUFFER_H