// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_H_

#include "scenic/render/rhi3d/impl/gl/prerequisites.h"

namespace scenic {
namespace detail {
class GlRenderDevice;

// Stub WGL swap-interval entry points (real procs live in VSyncFuncImpl).
class VSyncFunc {
 public:
  VSyncFunc() = default;
  virtual ~VSyncFunc() = default;
  virtual long Initialize(GlRenderDevice* /*pGLRenderDevice*/) {
    return kErrNone;
  }

  virtual int WaitForVSync() { return 0; }
  virtual void EnableVSync() {}
  virtual void DisableVSync() {}
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_VSYNC_FUNC_H_
