// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_H_
#define LEGACY_RENDER_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_H_

#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

namespace render {
class SmtGLRenderDevice;
typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

// Stub WGL swap-interval entry points (real procs live in SmtVSyncFuncImpl).
class SmtVSyncFunc {
 public:
  SmtVSyncFunc() = default;
  virtual ~SmtVSyncFunc() = default;
  virtual long Initialize(LPGLRENDERDEVICE /*pGLRenderDevice*/) {
    return SMT_ERR_NONE;
  }

  virtual int WaitForVSync() { return 0; }
  virtual void EnableVSync() {}
  virtual void DisableVSync() {}
};
}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_H_
