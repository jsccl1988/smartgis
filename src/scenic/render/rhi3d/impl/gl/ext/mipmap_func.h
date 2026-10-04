// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_H_

#include "scenic/render/rhi3d/impl/gl/prerequisites.h"

namespace scenic {
namespace detail {
class GlRenderDevice;

// Stub GL mipmap extension entry points (real procs live in MipmapFuncImpl).
class MipmapFunc {
 public:
  MipmapFunc() = default;
  virtual ~MipmapFunc() = default;
  virtual long Initialize(GlRenderDevice* /*pGLRenderDevice*/) {
    return kErrNone;
  }

  virtual void glGenerateMipmap(GLenum /*target*/) {}
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_H_
