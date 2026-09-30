// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_CAPS_DEVICECAPS_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_CAPS_DEVICECAPS_H_

#include "legacy/render/rhi3d/public/device/device_caps.h"

namespace render {

// Reports D3D11-era capability defaults for leftover Smt3DDeviceCaps queries.
class SmtD3DDeviceCaps : public Smt3DDeviceCaps {
 public:
  explicit SmtD3DDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice);
  ~SmtD3DDeviceCaps() override = default;

  bool IsVSyncSupported() override;
  bool IsAnisotropySupported() override;
  int GetTextureSlotsCount() override;
  int GetMaxColorAttachments() override;
  float GetMaxAnisotropy() override;
};

}  // namespace render

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_CAPS_DEVICECAPS_H_
