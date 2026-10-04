// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_3DDEVICECAPS_H
#define _RD3D_3DDEVICECAPS_H

namespace scenic {
namespace detail {
class RenderDevice3d;
typedef class RenderDevice3d *LP3DRENDERDEVICE;

class DeviceCaps3d {
 public:
  DeviceCaps3d(LP3DRENDERDEVICE p3DRenderDevice)
      : m_p3DRenderDevice(p3DRenderDevice) {};
  virtual ~DeviceCaps3d(void) {};

 public:
  virtual bool IsVSyncSupported() = 0;

  virtual bool IsAnisotropySupported() = 0;

  virtual int GetTextureSlotsCount() = 0;

  virtual int GetMaxColorAttachments() = 0;

  virtual float GetMaxAnisotropy() = 0;

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
};
}  // namespace detail
}  // namespace scenic

#endif  //_RD3D_3DDEVICECAPS_H
