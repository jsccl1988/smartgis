// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_IMP_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_IMP_H_

#include "scenic/render/rhi3d/impl/gl/ext/multitexture_func.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

namespace scenic {
namespace detail {

// Binds glActiveTextureARB via wglGetProcAddress.
class MultitextureFuncImpl : public MultitextureFunc {
 public:
  MultitextureFuncImpl() = default;
  ~MultitextureFuncImpl() override = default;

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
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_IMP_H_
