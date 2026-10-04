// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_IMP_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_IMP_H_

#include "scenic/render/rhi3d/impl/gl/ext/mipmap_func.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

namespace scenic {
namespace detail {

// Binds glGenerateMipmap via wglGetProcAddress.
class MipmapFuncImpl : public MipmapFunc {
 public:
  MipmapFuncImpl() = default;
  ~MipmapFuncImpl() override = default;

  long Initialize(GlRenderDevice* pGLRenderDevice) override {
    _glGenerateMipmap =
        (PFNGLGENERATEMIPMAPEXTPROC)pGLRenderDevice->GetProcAddress(
            "glGenerateMipmap");
    if (nullptr == _glGenerateMipmap) {
      return kErrFailure;
    }
    return kErrNone;
  }

  void glGenerateMipmap(GLenum target) override { _glGenerateMipmap(target); }

 private:
  PFNGLGENERATEMIPMAPEXTPROC _glGenerateMipmap = nullptr;
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_IMP_H_
