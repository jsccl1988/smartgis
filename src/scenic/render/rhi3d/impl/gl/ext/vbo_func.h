// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_VBO_FUNC_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_VBO_FUNC_H_

#include "scenic/render/rhi3d/impl/gl/prerequisites.h"

namespace scenic {
namespace detail {
class GlRenderDevice;
typedef class GlRenderDevice *LPGLRENDERDEVICE;

// Stub VBO entry points (real procs live in VboFuncImpl).
class VboFunc {
 public:
  VboFunc() = default;
  virtual ~VboFunc() = default;
  virtual long Initialize(LPGLRENDERDEVICE /*pGLRenderDevice*/) {
    return SMT_ERR_NONE;
  }

  virtual void glGenBuffers(GLsizei /*count*/, GLuint * /*handle*/) {}
  virtual void glDeleteBuffers(GLsizei /*count*/, const GLuint * /*handle*/) {}
  virtual void glBindBuffer(GLenum /*target*/, GLuint /*handle*/) {}
  virtual void glBufferData(GLenum /*target*/, GLsizeiptr /*size*/,
                            GLvoid * /*data*/, GLenum /*method*/) {}
  virtual void *glMapBuffer(GLenum /*target*/, GLenum /*access*/) {
    return nullptr;
  }
  virtual GLboolean glUnmapBuffer(GLenum /*target*/) { return GL_FALSE; }
  virtual void glGetBufferParameteriv(GLenum /*target*/, GLenum /*param*/,
                                      GLint * /*value*/) {}
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_VBO_FUNC_H_
