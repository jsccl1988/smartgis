// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI3D_IMPL_GL_EXT_VBO_FUNC_H_
#define LEGACY_RENDER_RHI3D_IMPL_GL_EXT_VBO_FUNC_H_

#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

namespace render {
class SmtGLRenderDevice;
typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

// Stub VBO entry points (real procs live in SmtVBOFuncImpl).
class SmtVBOFunc {
 public:
  SmtVBOFunc() = default;
  virtual ~SmtVBOFunc() = default;
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
}  // namespace render

#endif  // LEGACY_RENDER_RHI3D_IMPL_GL_EXT_VBO_FUNC_H_
