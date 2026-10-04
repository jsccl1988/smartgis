// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_SHADER_FUNC_IMP_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_SHADER_FUNC_IMP_H_

#include "scenic/render/rhi3d/impl/gl/ext/shader_func.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

namespace scenic {
namespace detail {

// Binds ARB shader-object entry points via wglGetProcAddress.
class ShadersFuncImpl : public ShadersFunc {
 public:
  ShadersFuncImpl() = default;
  ~ShadersFuncImpl() override = default;

  long Initialize(LPGLRENDERDEVICE pGLRenderDevice) override {
    _glCreateShader = (PFNGLCREATESHADERPROC)pGLRenderDevice->GetProcAddress(
        "glCreateShaderObjectARB");
    _glCreateProgram = (PFNGLCREATEPROGRAMPROC)pGLRenderDevice->GetProcAddress(
        "glCreateProgramObjectARB");
    _glAttachShader = (PFNGLATTACHSHADERPROC)pGLRenderDevice->GetProcAddress(
        "glAttachObjectARB");
    _glLinkProgram =
        (PFNGLLINKPROGRAMPROC)pGLRenderDevice->GetProcAddress("glLinkProgramARB");
    _glGetObjectParameteriv =
        (PFNGLGETOBJECTPARAMETERIVARBPROC)pGLRenderDevice->GetProcAddress(
            "glGetObjectParameterivARB");
    _glUseProgram = (PFNGLUSEPROGRAMPROC)pGLRenderDevice->GetProcAddress(
        "glUseProgramObjectARB");
    _glShaderSource = (PFNGLSHADERSOURCEARBPROC)pGLRenderDevice->GetProcAddress(
        "glShaderSourceARB");
    _glCompileShader =
        (PFNGLCOMPILESHADERARBPROC)pGLRenderDevice->GetProcAddress(
            "glCompileShaderARB");
    _glDetachShader = (PFNGLDETACHSHADERPROC)pGLRenderDevice->GetProcAddress(
        "glDetachObjectARB");
    _glDeleteObject = (PFNGLDELETEOBJECTARBPROC)pGLRenderDevice->GetProcAddress(
        "glDeleteObjectARB");
    _glGetInfoLog = (PFNGLGETINFOLOGARBPROC)pGLRenderDevice->GetProcAddress(
        "glGetInfoLogARB");
    _glGetUniformLocation =
        (PFNGLGETUNIFORMLOCATIONARBPROC)pGLRenderDevice->GetProcAddress(
            "glGetUniformLocationARB");
    _glUniform1fv = (PFNGLUNIFORM1FVARBPROC)pGLRenderDevice->GetProcAddress(
        "glUniform1fvARB");
    _glUniform2fv = (PFNGLUNIFORM2FVARBPROC)pGLRenderDevice->GetProcAddress(
        "glUniform2fvARB");
    _glUniform3fv = (PFNGLUNIFORM3FVARBPROC)pGLRenderDevice->GetProcAddress(
        "glUniform3fvARB");
    _glUniform4fv = (PFNGLUNIFORM4FVARBPROC)pGLRenderDevice->GetProcAddress(
        "glUniform4fvARB");
    _glUniform4f =
        (PFNGLUNIFORM4FARBPROC)pGLRenderDevice->GetProcAddress("glUniform4fARB");
    _glUniform1i =
        (PFNGLUNIFORM1IARBPROC)pGLRenderDevice->GetProcAddress("glUniform1iARB");
    _glGetUniformfv = (PFNGLGETUNIFORMFVARBPROC)pGLRenderDevice->GetProcAddress(
        "glGetUniformfvARB");
    _glGetUniformiv = (PFNGLGETUNIFORMIVARBPROC)pGLRenderDevice->GetProcAddress(
        "glGetUniformivARB");

    if (nullptr == _glCreateShader || nullptr == _glCreateProgram ||
        nullptr == _glAttachShader || nullptr == _glLinkProgram ||
        nullptr == _glGetObjectParameteriv || nullptr == _glUseProgram ||
        nullptr == _glShaderSource || nullptr == _glCompileShader ||
        nullptr == _glDetachShader || nullptr == _glDeleteObject ||
        nullptr == _glGetInfoLog || nullptr == _glGetUniformLocation ||
        nullptr == _glUniform1fv || nullptr == _glUniform2fv || nullptr == _glUniform3fv ||
        nullptr == _glUniform4fv || nullptr == _glUniform1i ||
        nullptr == _glGetUniformfv || nullptr == _glGetUniformiv) {
      return SMT_ERR_FAILURE;
    }
    return SMT_ERR_NONE;
  }

  GLuint glCreateShader(GLenum type) override { return _glCreateShader(type); }
  GLuint glCreateProgram() override { return _glCreateProgram(); }
  void glAttachShader(GLuint programId, GLuint shaderId) override {
    _glAttachShader(programId, shaderId);
  }
  void glLinkProgram(GLuint programId) override { _glLinkProgram(programId); }
  void glGetObjectParameteriv(GLhandleARB programId, GLenum target,
                              GLint *value) override {
    _glGetObjectParameteriv(programId, target, value);
  }
  void glUseProgram(GLuint programId) override { _glUseProgram(programId); }
  void glShaderSource(GLuint shaderId, GLsizei size, const GLchar **source,
                      const GLint *param) override {
    _glShaderSource(shaderId, size, source, param);
  }
  void glCompileShader(GLuint shaderId) override { _glCompileShader(shaderId); }
  void glDetachShader(GLuint programId, GLuint shaderId) override {
    _glDetachShader(programId, shaderId);
  }
  void glDeleteObject(GLhandleARB handle) override { _glDeleteObject(handle); }
  void glGetInfoLog(GLhandleARB programId, GLsizei size, GLsizei *written,
                    GLcharARB *log) override {
    _glGetInfoLog(programId, size, written, log);
  }
  GLint glGetUniformLocation(GLuint shaderId, const GLchar *name) override {
    return _glGetUniformLocation(shaderId, name);
  }
  void glUniform1fv(GLint paramId, GLsizei size,
                    const GLfloat *value) override {
    _glUniform1fv(paramId, size, value);
  }
  void glUniform2fv(GLint paramId, GLsizei size,
                    const GLfloat *value) override {
    _glUniform2fv(paramId, size, value);
  }
  void glUniform3fv(GLint paramId, GLsizei size,
                    const GLfloat *value) override {
    _glUniform3fv(paramId, size, value);
  }
  void glUniform4fv(GLint paramId, GLsizei size,
                    const GLfloat *value) override {
    _glUniform4fv(paramId, size, value);
  }
  void glUniform4f(GLint paramId, float a, float b, float c,
                   float d) override {
    _glUniform4f(paramId, a, b, c, d);
  }
  void glUniform1i(GLint paramId, GLint value) override {
    _glUniform1i(paramId, value);
  }
  void glGetUniformfv(GLuint shaderId, GLint paramId,
                      GLfloat *values) override {
    _glGetUniformfv(shaderId, paramId, values);
  }
  void glGetUniformiv(GLuint shaderId, GLint paramId, GLint *values) override {
    _glGetUniformiv(shaderId, paramId, values);
  }

 private:
  PFNGLCREATESHADERPROC _glCreateShader = nullptr;
  PFNGLCREATEPROGRAMPROC _glCreateProgram = nullptr;
  PFNGLATTACHSHADERPROC _glAttachShader = nullptr;
  PFNGLLINKPROGRAMPROC _glLinkProgram = nullptr;
  PFNGLGETOBJECTPARAMETERIVARBPROC _glGetObjectParameteriv = nullptr;
  PFNGLUSEPROGRAMPROC _glUseProgram = nullptr;
  PFNGLSHADERSOURCEARBPROC _glShaderSource = nullptr;
  PFNGLCOMPILESHADERARBPROC _glCompileShader = nullptr;
  PFNGLDETACHSHADERPROC _glDetachShader = nullptr;
  PFNGLDELETEOBJECTARBPROC _glDeleteObject = nullptr;
  PFNGLGETINFOLOGARBPROC _glGetInfoLog = nullptr;
  PFNGLGETUNIFORMLOCATIONARBPROC _glGetUniformLocation = nullptr;
  PFNGLUNIFORM1FVARBPROC _glUniform1fv = nullptr;
  PFNGLUNIFORM2FVARBPROC _glUniform2fv = nullptr;
  PFNGLUNIFORM3FVARBPROC _glUniform3fv = nullptr;
  PFNGLUNIFORM4FVARBPROC _glUniform4fv = nullptr;
  PFNGLUNIFORM4FARBPROC _glUniform4f = nullptr;
  PFNGLUNIFORM1IARBPROC _glUniform1i = nullptr;
  PFNGLGETUNIFORMFVARBPROC _glGetUniformfv = nullptr;
  PFNGLGETUNIFORMIVARBPROC _glGetUniformiv = nullptr;
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_SHADER_FUNC_IMP_H_
