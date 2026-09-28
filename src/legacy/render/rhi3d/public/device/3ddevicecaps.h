// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_3DDEVICECAPS_H
#define _RD3D_3DDEVICECAPS_H

namespace render {
class Smt3DRenderDevice;
typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

class Smt3DDeviceCaps {
 public:
  Smt3DDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice)
      : m_p3DRenderDevice(p3DRenderDevice) {};
  virtual ~Smt3DDeviceCaps(void) {};

 public:
  virtual bool IsVSyncSupported() = 0;

  virtual bool IsAnisotropySupported() = 0;

  virtual int GetTextureSlotsCount() = 0;

  virtual int GetMaxColorAttachments() = 0;

  virtual float GetMaxAnisotropy() = 0;

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
};
}  // namespace render

#endif  //_RD3D_3DDEVICECAPS_H
