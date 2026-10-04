// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_H_

#include "scenic/render/rhi3d/impl/gl/prerequisites.h"

namespace scenic {
namespace detail {
class GlRenderDevice;

// Stub multitexture entry points (real procs live in MultitextureFuncImpl).
class MultitextureFunc {
 public:
  MultitextureFunc() = default;
  virtual ~MultitextureFunc() = default;

  virtual long Initialize(GlRenderDevice* /*pGLRenderDevice*/) {
    return kErrNone;
  }

  virtual void glActiveTexture(GLenum /*texture*/) {}
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_H_
