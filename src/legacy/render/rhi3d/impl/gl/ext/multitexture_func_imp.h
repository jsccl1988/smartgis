// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_IMP_H_
#define LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_IMP_H_

#include "legacy/render/rhi3d/impl/gl/ext/multitexture_func.h"
#include "legacy/render/rhi3d/impl/gl/host/render_device.h"

namespace render {

// Binds glActiveTextureARB via wglGetProcAddress.
class SmtMultitextureFuncImpl : public SmtMultitextureFunc {
 public:
  SmtMultitextureFuncImpl() = default;
  ~SmtMultitextureFuncImpl() override = default;

  long Initialize(LPGLRENDERDEVICE pGLRenderDevice) override {
    _glActiveTexture = (PFNGLACTIVETEXTUREPROC)pGLRenderDevice->GetProcAddress(
        "glActiveTextureARB");
    if (nullptr == _glActiveTexture) {
      return SMT_ERR_FAILURE;
    }
    return SMT_ERR_NONE;
  }

  void glActiveTexture(GLenum texture) override { _glActiveTexture(texture); }

 private:
  PFNGLACTIVETEXTUREPROC _glActiveTexture = nullptr;
};
}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_IMP_H_
