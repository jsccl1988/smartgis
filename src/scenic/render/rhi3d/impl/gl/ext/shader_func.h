// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_GL_EXT_SHADER_FUNC_H_
#define SCENIC_RHI3D_IMPL_GL_EXT_SHADER_FUNC_H_

#include "scenic/render/rhi3d/impl/gl/prerequisites.h"

namespace scenic {
namespace detail {
class GlRenderDevice;

// Stub GLSL/ARB shader entry points (real procs live in ShadersFuncImpl).
class ShadersFunc {
 public:
  ShadersFunc() = default;
  virtual ~ShadersFunc() = default;
  virtual long Initialize(GlRenderDevice* /*pGLRenderDevice*/) {
    return kErrNone;
  }

  virtual GLuint glCreateShader(GLenum /*type*/) { return 0; }
  virtual GLuint glCreateProgram() { return 0; }
  virtual void glAttachShader(GLuint /*programId*/, GLuint /*shaderId*/) {}
  virtual void glLinkProgram(GLuint /*programId*/) {}
  virtual void glGetObjectParameteriv(GLhandleARB /*programId*/,
                                      GLenum /*target*/, GLint * /*value*/) {}
  virtual void glUseProgram(GLuint /*programID*/) {}
  virtual void glShaderSource(GLuint /*shaderId*/, GLsizei /*size*/,
                              const GLchar ** /*source*/,
                              const GLint * /*param*/) {}
  virtual void glCompileShader(GLuint /*shaderId*/) {}
  virtual void glDetachShader(GLuint /*programId*/, GLuint /*shaderId*/) {}
  virtual void glDeleteObject(GLhandleARB /*handle*/) {}
  virtual void glGetInfoLog(GLhandleARB /*programId*/, GLsizei /*size*/,
                            GLsizei * /*written*/, GLcharARB * /*log*/) {}
  virtual GLint glGetUniformLocation(GLuint /*shaderId*/, const char * /*name*/) {
    return 0;
  }
  virtual void glUniform1fv(GLint /*paramId*/, GLsizei /*size*/,
                            const GLfloat * /*value*/) {}
  virtual void glUniform2fv(GLint /*paramId*/, GLsizei /*size*/,
                            const GLfloat * /*value*/) {}
  virtual void glUniform3fv(GLint /*paramId*/, GLsizei /*size*/,
                            const GLfloat * /*value*/) {}
  virtual void glUniform4fv(GLint /*paramId*/, GLsizei /*size*/,
                            const GLfloat * /*value*/) {}
  virtual void glUniform4f(GLint /*paramId*/, float /*a*/, float /*b*/,
                           float /*c*/, float /*d*/) {}
  virtual void glUniform1i(GLint /*paramId*/, GLint /*value*/) {}
  virtual void glGetUniformfv(GLuint /*shaderId*/, GLint /*paramId*/,
                              GLfloat * /*values*/) {}
  virtual void glGetUniformiv(GLuint /*shaderId*/, GLint /*paramId*/,
                              GLint * /*values*/) {}
};
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_GL_EXT_SHADER_FUNC_H_
