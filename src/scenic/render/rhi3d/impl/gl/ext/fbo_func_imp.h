// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_FBO_FUNC_IMP_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_FBO_FUNC_IMP_H_

#include "scenic/render/rhi3d/impl/gl/ext/fbo_func.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

namespace scenic {
namespace detail {

// Binds EXT framebuffer-object entry points via wglGetProcAddress.
class FboFuncImpl : public FboFunc {
 public:
  FboFuncImpl() = default;
  ~FboFuncImpl() override = default;

  long Initialize(GlRenderDevice* pGLRenderDevice) override {
    _glGenFramebuffers =
        (PFNGLGENFRAMEBUFFERSEXTPROC)pGLRenderDevice->GetProcAddress(
            "glGenFramebuffersEXT");
    _glDeleteFramebuffers =
        (PFNGLDELETEFRAMEBUFFERSEXTPROC)pGLRenderDevice->GetProcAddress(
            "glDeleteFramebuffersEXT");
    _glBindFramebuffer =
        (PFNGLBINDFRAMEBUFFEREXTPROC)pGLRenderDevice->GetProcAddress(
            "glBindFramebufferEXT");
    _glIsFramebuffer =
        (PFNGLISFRAMEBUFFEREXTPROC)pGLRenderDevice->GetProcAddress(
            "glIsFramebufferEXT");
    _glGenRenderbuffers =
        (PFNGLGENRENDERBUFFERSEXTPROC)pGLRenderDevice->GetProcAddress(
            "glGenRenderbuffersEXT");
    _glDeleteRenderbuffers =
        (PFNGLDELETERENDERBUFFERSEXTPROC)pGLRenderDevice->GetProcAddress(
            "glDeleteRenderbuffersEXT");
    _glBindRenderbuffer =
        (PFNGLBINDRENDERBUFFEREXTPROC)pGLRenderDevice->GetProcAddress(
            "glBindRenderbufferEXT");
    _glIsRenderbuffer =
        (PFNGLISRENDERBUFFEREXTPROC)pGLRenderDevice->GetProcAddress(
            "glIsRenderbufferEXT");
    _glRenderbufferStorage =
        (PFNGLRENDERBUFFERSTORAGEEXTPROC)pGLRenderDevice->GetProcAddress(
            "glRenderbufferStorageEXT");
    _glFramebufferRenderbuffer =
        (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)pGLRenderDevice->GetProcAddress(
            "glFramebufferRenderbufferEXT");
    _glFramebufferTexture1D =
        (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC)pGLRenderDevice->GetProcAddress(
            "glFramebufferTexture1D");
    _glFramebufferTexture2D =
        (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)pGLRenderDevice->GetProcAddress(
            "glFramebufferTexture2D");
    _glFramebufferTexture3D =
        (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC)pGLRenderDevice->GetProcAddress(
            "glFramebufferTexture3D");
    _glCheckFramebufferStatus =
        (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)pGLRenderDevice->GetProcAddress(
            "glCheckFramebufferStatusEXT");

    if (nullptr == _glGenFramebuffers || nullptr == _glDeleteFramebuffers ||
        nullptr == _glBindFramebuffer || nullptr == _glIsFramebuffer ||
        nullptr == _glGenRenderbuffers || nullptr == _glDeleteRenderbuffers ||
        nullptr == _glBindRenderbuffer || nullptr == _glIsRenderbuffer ||
        nullptr == _glRenderbufferStorage || nullptr == _glFramebufferRenderbuffer ||
        nullptr == _glCheckFramebufferStatus) {
      return kErrFailure;
    }
    return kErrNone;
  }

  void glGenFramebuffers(GLsizei count, GLuint *ids) override {
    _glGenFramebuffers(count, ids);
  }
  void glDeleteFramebuffers(GLsizei count, GLuint *ids) override {
    _glDeleteFramebuffers(count, ids);
  }
  void glBindFramebuffer(GLenum target, GLuint id) override {
    _glBindFramebuffer(target, id);
  }
  GLboolean glIsFramebuffer(GLuint id) override {
    return _glIsFramebuffer(id);
  }
  void glGenRenderbuffers(GLsizei count, GLuint *ids) override {
    _glGenRenderbuffers(count, ids);
  }
  void glDeleteRenderbuffers(GLsizei count, GLuint *ids) override {
    _glDeleteRenderbuffers(count, ids);
  }
  void glBindRenderbuffer(GLenum target, GLuint id) override {
    _glBindRenderbuffer(target, id);
  }
  GLboolean glIsRenderbuffer(GLuint id) override {
    return _glIsRenderbuffer(id);
  }
  void glRenderbufferStorage(GLenum target, GLenum internalFormat,
                             GLsizei width, GLsizei height) override {
    _glRenderbufferStorage(target, internalFormat, width, height);
  }
  void glFramebufferRenderbuffer(GLenum target, GLenum attachment,
                                 GLenum rbTarget, GLuint rbId) override {
    _glFramebufferRenderbuffer(target, attachment, rbTarget, rbId);
  }
  int getMaxColorAttachments() override {
    int maxColors = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxColors);
    return maxColors;
  }
  void glFramebufferTexture1D(GLenum target, GLenum attachment,
                              GLenum texTarget, GLuint texId,
                              int level) override {
    _glFramebufferTexture1D(target, attachment, texTarget, texId, level);
  }
  void glFramebufferTexture2D(GLenum target, GLenum attachment,
                              GLenum texTarget, GLuint texId,
                              int level) override {
    _glFramebufferTexture2D(target, attachment, texTarget, texId, level);
  }
  void glFramebufferTexture3D(GLenum target, GLenum attachment,
                              GLenum texTarget, GLuint texId, int level,
                              int zOffset) override {
    _glFramebufferTexture3D(target, attachment, texTarget, texId, level,
                            zOffset);
  }
  GLenum glCheckFramebufferStatus(GLenum target) override {
    return _glCheckFramebufferStatus(target);
  }

 private:
  PFNGLGENFRAMEBUFFERSEXTPROC _glGenFramebuffers = nullptr;
  PFNGLDELETEFRAMEBUFFERSEXTPROC _glDeleteFramebuffers = nullptr;
  PFNGLBINDFRAMEBUFFEREXTPROC _glBindFramebuffer = nullptr;
  PFNGLISFRAMEBUFFEREXTPROC _glIsFramebuffer = nullptr;
  PFNGLGENRENDERBUFFERSEXTPROC _glGenRenderbuffers = nullptr;
  PFNGLDELETERENDERBUFFERSEXTPROC _glDeleteRenderbuffers = nullptr;
  PFNGLBINDRENDERBUFFEREXTPROC _glBindRenderbuffer = nullptr;
  PFNGLISRENDERBUFFEREXTPROC _glIsRenderbuffer = nullptr;
  PFNGLRENDERBUFFERSTORAGEEXTPROC _glRenderbufferStorage = nullptr;
  PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC _glFramebufferRenderbuffer = nullptr;
  PFNGLFRAMEBUFFERTEXTURE1DEXTPROC _glFramebufferTexture1D = nullptr;
  PFNGLFRAMEBUFFERTEXTURE2DEXTPROC _glFramebufferTexture2D = nullptr;
  PFNGLFRAMEBUFFERTEXTURE3DEXTPROC _glFramebufferTexture3D = nullptr;
  PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC _glCheckFramebufferStatus = nullptr;
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_FBO_FUNC_IMP_H_
