// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_CAPS_DEVICECAPS_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_CAPS_DEVICECAPS_H_

#include "scenic/render/rhi3d/impl/d3d/prerequisites.h"
#include "scenic/render/rhi3d/public/device/device_caps.h"

namespace scenic {
namespace detail {

// Reports D3D11-era capability defaults for leftover DeviceCaps3d queries.
class D3dDeviceCaps : public DeviceCaps3d {
 public:
  explicit D3dDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice)
      : DeviceCaps3d(p3DRenderDevice) {}
  ~D3dDeviceCaps() override = default;

  bool IsVSyncSupported() override { return true; }
  bool IsAnisotropySupported() override { return true; }
  int GetTextureSlotsCount() override {
    // D3D11 common-shader slot count for pixel shaders.
    return 16;
  }
  int GetMaxColorAttachments() override {
    return D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
  }
  float GetMaxAnisotropy() override {
    return static_cast<float>(D3D11_REQ_MAXANISOTROPY);
  }
};

}  // namespace detail
}  // namespace scenic

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_CAPS_DEVICECAPS_H_
