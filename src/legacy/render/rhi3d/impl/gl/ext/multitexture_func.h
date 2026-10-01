// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_H_
#define LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_H_

#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

namespace render {
class SmtGLRenderDevice;
typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

// Stub multitexture entry points (real procs live in SmtMultitextureFuncImpl).
class SmtMultitextureFunc {
 public:
  SmtMultitextureFunc() = default;
  virtual ~SmtMultitextureFunc() = default;

  virtual long Initialize(LPGLRENDERDEVICE /*pGLRenderDevice*/) {
    return SMT_ERR_NONE;
  }

  virtual void glActiveTexture(GLenum /*texture*/) {}
};
}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_GL_EXT_MULTITEXTURE_FUNC_H_
