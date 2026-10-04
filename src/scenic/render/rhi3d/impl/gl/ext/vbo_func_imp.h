// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_VBO_FUNC_IMP_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_VBO_FUNC_IMP_H_

#include "scenic/render/rhi3d/impl/gl/ext/vbo_func.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

namespace scenic {
namespace detail {

// Binds ARB VBO entry points via wglGetProcAddress.
class VboFuncImpl : public VboFunc {
 public:
  VboFuncImpl() = default;
  ~VboFuncImpl() override = default;

  long Initialize(GlRenderDevice* pGLRenderDevice) override {
    _glGenBuffers =
        (PFNGLGENBUFFERSPROC)pGLRenderDevice->GetProcAddress("glGenBuffersARB");
    _glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)pGLRenderDevice->GetProcAddress(
        "glDeleteBuffersARB");
    _glBindBuffer =
        (PFNGLBINDBUFFERPROC)pGLRenderDevice->GetProcAddress("glBindBufferARB");
    _glBufferData =
        (PFNGLBUFFERDATAPROC)pGLRenderDevice->GetProcAddress("glBufferDataARB");
    _glMapBuffer =
        (PFNGLMAPBUFFERPROC)pGLRenderDevice->GetProcAddress("glMapBufferARB");
    _glUnmapBuffer =
        (PFNGLUNMAPBUFFERPROC)pGLRenderDevice->GetProcAddress("glUnmapBufferARB");
    _glGetBufferParameteriv =
        (PFNGLGETBUFFERPARAMETERIVPROC)pGLRenderDevice->GetProcAddress(
            "glGetBufferParameterivARB");

    if (nullptr == _glGenBuffers || nullptr == _glDeleteBuffers ||
        nullptr == _glBindBuffer || nullptr == _glBufferData || nullptr == _glMapBuffer ||
        nullptr == _glUnmapBuffer || nullptr == _glGetBufferParameteriv) {
      return kErrFailure;
    }
    return kErrNone;
  }

  void glGenBuffers(GLsizei count, GLuint *handle) override {
    _glGenBuffers(count, handle);
  }
  void glDeleteBuffers(GLsizei count, const GLuint *handle) override {
    _glDeleteBuffers(count, handle);
  }
  void glBindBuffer(GLenum target, GLuint handle) override {
    _glBindBuffer(target, handle);
  }
  void glBufferData(GLenum target, GLsizeiptr size, GLvoid *data,
                    GLenum method) override {
    _glBufferData(target, size, data, method);
  }
  void *glMapBuffer(GLenum target, GLenum access) override {
    return _glMapBuffer(target, access);
  }
  GLboolean glUnmapBuffer(GLenum target) override {
    return _glUnmapBuffer(target);
  }
  void glGetBufferParameteriv(GLenum target, GLenum param,
                              GLint *value) override {
    _glGetBufferParameteriv(target, param, value);
  }

 private:
  PFNGLGENBUFFERSPROC _glGenBuffers = nullptr;
  PFNGLDELETEBUFFERSPROC _glDeleteBuffers = nullptr;
  PFNGLBINDBUFFERPROC _glBindBuffer = nullptr;
  PFNGLBUFFERDATAPROC _glBufferData = nullptr;
  PFNGLMAPBUFFERPROC _glMapBuffer = nullptr;
  PFNGLUNMAPBUFFERPROC _glUnmapBuffer = nullptr;
  PFNGLGETBUFFERPARAMETERIVPROC _glGetBufferParameteriv = nullptr;
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_VBO_FUNC_IMP_H_
