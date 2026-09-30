// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/caps/device_caps.h"

#include "legacy/render/rhi3d/impl/d3d/prerequisites.h"

namespace render {

SmtD3DDeviceCaps::SmtD3DDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice)
    : Smt3DDeviceCaps(p3DRenderDevice) {}

bool SmtD3DDeviceCaps::IsVSyncSupported() { return true; }

bool SmtD3DDeviceCaps::IsAnisotropySupported() { return true; }

int SmtD3DDeviceCaps::GetTextureSlotsCount() {
  // D3D11 common-shader slot count for pixel shaders.
  return 16;
}

int SmtD3DDeviceCaps::GetMaxColorAttachments() {
  return D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
}

float SmtD3DDeviceCaps::GetMaxAnisotropy() {
  return static_cast<float>(D3D11_REQ_MAXANISOTROPY);
}

}  // namespace render
