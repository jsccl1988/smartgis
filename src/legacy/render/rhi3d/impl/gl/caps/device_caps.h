// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_GL_CAPS_DEVICE_CAPS_H_
#define LEGACY_RENDER_RHI3D_IMPL_GL_CAPS_DEVICE_CAPS_H_

#include "legacy/render/rhi3d/impl/gl/prerequisites.h"
#include "legacy/render/rhi3d/public/device/device_caps.h"

namespace render {

class SmtGLRenderDevice;

// Reports leftover GL capability queries from extension strings / GL gets.
class SmtGLDeviceCaps : public Smt3DDeviceCaps {
 public:
  explicit SmtGLDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice);
  ~SmtGLDeviceCaps() override;

  bool IsVSyncSupported() override;
  bool IsAnisotropySupported() override;
  bool IsVBOSupported();
  bool IsMipMapsSupported();
  bool IsFBOSupported();
  bool IsGLSLSupported();
  bool IsMultiTextureSupported();
  int GetTextureSlotsCount() override;
  int GetMaxColorAttachments() override;
  float GetMaxAnisotropy() override;
};
}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_GL_CAPS_DEVICE_CAPS_H_
