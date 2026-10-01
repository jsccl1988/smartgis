// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_CAPS_DEVICECAPS_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_CAPS_DEVICECAPS_H_

#include "legacy/render/rhi3d/impl/d3d/prerequisites.h"
#include "legacy/render/rhi3d/public/device/device_caps.h"

namespace render {

// Reports D3D11-era capability defaults for leftover Smt3DDeviceCaps queries.
class SmtD3DDeviceCaps : public Smt3DDeviceCaps {
 public:
  explicit SmtD3DDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice)
      : Smt3DDeviceCaps(p3DRenderDevice) {}
  ~SmtD3DDeviceCaps() override = default;

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

}  // namespace render

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_CAPS_DEVICECAPS_H_
