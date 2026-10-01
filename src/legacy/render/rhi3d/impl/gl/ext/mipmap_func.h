// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_H_
#define LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_H_

#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

namespace render {
class SmtGLRenderDevice;
typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

// Stub GL mipmap extension entry points (real procs live in SmtMipmapFuncImpl).
class SmtMipmapFunc {
 public:
  SmtMipmapFunc() = default;
  virtual ~SmtMipmapFunc() = default;
  virtual long Initialize(LPGLRENDERDEVICE /*pGLRenderDevice*/) {
    return SMT_ERR_NONE;
  }

  virtual void glGenerateMipmap(GLenum /*target*/) {}
};
}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MIPMAP_FUNC_H_
