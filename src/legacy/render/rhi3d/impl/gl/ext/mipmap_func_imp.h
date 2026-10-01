// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_IMP_H_
#define LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_IMP_H_

#include "legacy/render/rhi3d/impl/gl/ext/mipmap_func.h"
#include "legacy/render/rhi3d/impl/gl/host/render_device.h"

namespace render {

// Binds glGenerateMipmap via wglGetProcAddress.
class SmtMipmapFuncImpl : public SmtMipmapFunc {
 public:
  SmtMipmapFuncImpl() = default;
  ~SmtMipmapFuncImpl() override = default;

  long Initialize(LPGLRENDERDEVICE pGLRenderDevice) override {
    _glGenerateMipmap =
        (PFNGLGENERATEMIPMAPEXTPROC)pGLRenderDevice->GetProcAddress(
            "glGenerateMipmap");
    if (nullptr == _glGenerateMipmap) {
      return SMT_ERR_FAILURE;
    }
    return SMT_ERR_NONE;
  }

  void glGenerateMipmap(GLenum target) override { _glGenerateMipmap(target); }

 private:
  PFNGLGENERATEMIPMAPEXTPROC _glGenerateMipmap = nullptr;
};
}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_IMP_H_
