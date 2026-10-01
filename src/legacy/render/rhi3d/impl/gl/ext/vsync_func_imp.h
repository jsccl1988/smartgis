// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_IMP_H_
#define LEGACY_RENDER_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_IMP_H_

#include "legacy/render/rhi3d/impl/gl/ext/vsync_func.h"
#include "legacy/render/rhi3d/impl/gl/host/render_device.h"

namespace render {

// Binds wglSwapIntervalEXT for vsync on/off.
class SmtVSyncFuncImpl : public SmtVSyncFunc {
 public:
  SmtVSyncFuncImpl() = default;
  ~SmtVSyncFuncImpl() override = default;

  long Initialize(LPGLRENDERDEVICE pGLRenderDevice) override {
    _wglSwapInterval =
        (PFNWGLSWAPINTERVALEXTPROC)pGLRenderDevice->GetProcAddress(
            "wglSwapIntervalEXT");
    if (nullptr == _wglSwapInterval) {
      return SMT_ERR_FAILURE;
    }
    return SMT_ERR_NONE;
  }

  int WaitForVSync() override { return 0; }
  void EnableVSync() override { _wglSwapInterval(1); }
  void DisableVSync() override { _wglSwapInterval(0); }

 private:
  PFNWGLSWAPINTERVALEXTPROC _wglSwapInterval = nullptr;
};
}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_IMP_H_
