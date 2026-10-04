// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_IMP_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_IMP_H_

#include "scenic/render/rhi3d/impl/gl/ext/vsync_func.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

namespace scenic {
namespace detail {

// Binds wglSwapIntervalEXT for vsync on/off.
class VSyncFuncImpl : public VSyncFunc {
 public:
  VSyncFuncImpl() = default;
  ~VSyncFuncImpl() override = default;

  long Initialize(GlRenderDevice* pGLRenderDevice) override {
    _wglSwapInterval =
        (PFNWGLSWAPINTERVALEXTPROC)pGLRenderDevice->GetProcAddress(
            "wglSwapIntervalEXT");
    if (nullptr == _wglSwapInterval) {
      return kErrFailure;
    }
    return kErrNone;
  }

  int WaitForVSync() override { return 0; }
  void EnableVSync() override { _wglSwapInterval(1); }
  void DisableVSync() override { _wglSwapInterval(0); }

 private:
  PFNWGLSWAPINTERVALEXTPROC _wglSwapInterval = nullptr;
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_IMP_H_
