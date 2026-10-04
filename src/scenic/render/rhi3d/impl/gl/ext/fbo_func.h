// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_FBO_FUNC_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_FBO_FUNC_H_

#include "scenic/render/rhi3d/impl/gl/prerequisites.h"

namespace scenic {
namespace detail {
class GlRenderDevice;

// Stub FBO entry points (real procs live in FboFuncImpl).
class FboFunc {
 public:
  FboFunc() = default;
  virtual ~FboFunc() = default;
  virtual long Initialize(GlRenderDevice* /*pGLRenderDevice*/) {
    return kErrNone;
  }

  virtual void glGenFramebuffers(GLsizei /*count*/, GLuint * /*ids*/) {}
  virtual void glDeleteFramebuffers(GLsizei /*count*/, GLuint * /*ids*/) {}
  virtual void glBindFramebuffer(GLenum /*target*/, GLuint /*id*/) {}
  virtual GLboolean glIsFramebuffer(GLuint /*id*/) { return GL_FALSE; }
  virtual void glGenRenderbuffers(GLsizei /*count*/, GLuint * /*ids*/) {}
  virtual void glDeleteRenderbuffers(GLsizei /*count*/, GLuint * /*ids*/) {}
  virtual void glBindRenderbuffer(GLenum /*target*/, GLuint /*id*/) {}
  virtual GLboolean glIsRenderbuffer(GLuint /*id*/) { return GL_FALSE; }
  virtual void glRenderbufferStorage(GLenum /*target*/,
                                     GLenum /*internalFormat*/,
                                     GLsizei /*width*/, GLsizei /*height*/) {}
  virtual void glFramebufferRenderbuffer(GLenum /*target*/,
                                         GLenum /*attachment*/,
                                         GLenum /*rbTarget*/, GLuint /*rbId*/) {
  }
  virtual int getMaxColorAttachments() { return 0; }
  virtual void glFramebufferTexture1D(GLenum /*target*/, GLenum /*attachment*/,
                                      GLenum /*texTarget*/, GLuint /*texId*/,
                                      int /*level*/) {}
  virtual void glFramebufferTexture2D(GLenum /*target*/, GLenum /*attachment*/,
                                      GLenum /*texTarget*/, GLuint /*texId*/,
                                      int /*level*/) {}
  virtual void glFramebufferTexture3D(GLenum /*target*/, GLenum /*attachment*/,
                                      GLenum /*texTarget*/, GLuint /*texId*/,
                                      int /*level*/, int /*zOffset*/) {}
  virtual GLenum glCheckFramebufferStatus(GLenum /*target*/) { return 0; }
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_FBO_FUNC_H_
