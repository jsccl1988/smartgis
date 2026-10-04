// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_CAPS_DEVICE_CAPS_H_
#define SCENIC_RHI3D_IMPL_GL_CAPS_DEVICE_CAPS_H_

#include "scenic/render/rhi3d/impl/gl/prerequisites.h"
#include "scenic/render/rhi3d/public/device/device_caps.h"

namespace scenic {
namespace detail {

class GlRenderDevice;

// Reports leftover GL capability queries from extension strings / GL gets.
class GlDeviceCaps : public DeviceCaps3d {
 public:
  explicit GlDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice);
  ~GlDeviceCaps() override;

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
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_CAPS_DEVICE_CAPS_H_
