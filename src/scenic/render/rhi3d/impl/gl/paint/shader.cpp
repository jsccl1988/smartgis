// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

#include <memory>

#include "base/core/log.h"
#include "scenic/render/rhi3d/public/shader/program_manager.h"
#include "scenic/render/rhi3d/public/shader/shader_manager.h"

namespace scenic {
namespace detail {
// vertex shader
Shader *GlRenderDevice::CreateVertexShader(const char *szName) {
  GLhandleARB newHandle = m_pFuncShaders->glCreateShader(GL_VERTEX_SHADER_ARB);
  auto shader = std::make_unique<Shader>(this, newHandle, szName);
  if (kErrNone == m_shaderMgr.AddShader(shader.get())) {
    return shader.release();
  }
  return nullptr;
}

// pixel shader
Shader *GlRenderDevice::CreatePixelShader(const char *szName) {
  GLhandleARB newHandle =
      m_pFuncShaders->glCreateShader(GL_FRAGMENT_SHADER_ARB);
  auto shader = std::make_unique<Shader>(this, newHandle, szName);
  if (kErrNone == m_shaderMgr.AddShader(shader.get())) {
    return shader.release();
  }
  return nullptr;
}

Shader *GlRenderDevice::GetShader(const char *szName) {
  return m_shaderMgr.GetShader(szName);
}

// program
Program *GlRenderDevice::CreateProgram(const char *szName) {
  GLhandleARB newHandle = m_pFuncShaders->glCreateProgram();
  auto program = std::make_unique<Program>(this, newHandle, szName);
  if (kErrNone == m_progamMgr.AddProgram(program.get())) {
    return program.release();
  }
  return nullptr;
}

Program *GlRenderDevice::GetProgram(const char *szName) {
  return m_progamMgr.GetProgram(szName);
}

long GlRenderDevice::LoadShaderSource(Shader *shader, char *source) {
  GLhandleARB handle = shader->GetHandle();
  m_pFuncShaders->glShaderSource(handle, 1, (const GLchar **)&source, nullptr);

  return kErrNone;
}

long GlRenderDevice::CompileShader(Shader *shader) {
  GLhandleARB handle = shader->GetHandle();
  m_pFuncShaders->glCompileShader(handle);

  return kErrNone;
}

long GlRenderDevice::IsShaderCompiled(Shader *shader) {
  GLhandleARB handle = shader->GetHandle();
  int compileStatus = 0;

  m_pFuncShaders->glGetObjectParameteriv(handle, GL_OBJECT_COMPILE_STATUS_ARB,
                                         &compileStatus);
  if (compileStatus == 0) {
    return kErrFailure;
  }

  return kErrNone;
}

char *GlRenderDevice::GetShaderLog(Shader *shader) {
  if (nullptr == shader) return nullptr;

  GLchar *log = nullptr;
  int logLength = 0;
  int charsWritten = 0;
  GLhandleARB handle = shader->GetHandle();

  m_pFuncShaders->glGetObjectParameteriv(handle, GL_OBJECT_INFO_LOG_LENGTH_ARB,
                                         &logLength);

  if (logLength < 1) return nullptr;

  log = new GLchar[logLength];

  m_pFuncShaders->glGetInfoLog(handle, logLength, &charsWritten, log);

  return log;
}

long GlRenderDevice::DestroyShader(const char *szName) {
  Shader *shader = m_shaderMgr.GetShader(szName);
  if (nullptr == shader) return kErrFailure;

  GLhandleARB handle = shader->GetHandle();
  if (handle != 0) {
    m_pFuncShaders->glDeleteObject(handle);
  }

  m_shaderMgr.DestroyShader(shader->GetShaderName());

  return kErrNone;
}

long GlRenderDevice::BindProgram(Program *program) {
  if (nullptr == program) return kErrFailure;

  m_pFuncShaders->glUseProgram(program->GetHandle());

  return kErrNone;
}

long GlRenderDevice::UnbindProgram() {
  m_pFuncShaders->glUseProgram(0);

  return kErrNone;
}

long GlRenderDevice::SetProgramVertexShader(Program *program,
                                               Shader *shader) {
  if (nullptr == program || nullptr == shader) return kErrFailure;

  m_pFuncShaders->glAttachShader(program->GetHandle(), shader->GetHandle());

  return kErrNone;
}

long GlRenderDevice::SetProgramPixelShader(Program *program,
                                              Shader *shader) {
  if (nullptr == program || nullptr == shader) return kErrFailure;

  m_pFuncShaders->glAttachShader(program->GetHandle(), shader->GetHandle());

  return kErrNone;
}

long GlRenderDevice::LinkProgram(Program *program) {
  if (nullptr == program) return kErrFailure;

  m_pFuncShaders->glLinkProgram(program->GetHandle());

  return kErrNone;
}

long GlRenderDevice::IsProgramLinked(Program *program) {
  if (nullptr == program) return kErrFailure;

  int linkStatus = 0;

  m_pFuncShaders->glGetObjectParameteriv(
      program->GetHandle(), GL_OBJECT_LINK_STATUS_ARB, &linkStatus);
  if (linkStatus == 0) {
    return kErrFailure;
  }

  return kErrNone;
}

char *GlRenderDevice::GetProgramLinkLog(Program *program) {
  if (nullptr == program) return nullptr;

  int logLength;
  char *log = nullptr;
  GLhandleARB handle = program->GetHandle();
  m_pFuncShaders->glGetObjectParameteriv(handle, GL_OBJECT_INFO_LOG_LENGTH_ARB,
                                         &logLength);

  if (logLength != 1) {
    log = new GLchar[logLength];
    int charsWritten;

    m_pFuncShaders->glGetInfoLog(handle, logLength, &charsWritten, log);
  }

  return log;
}

long GlRenderDevice::DestroyProgram(const char *szName) {
  Program *program = m_progamMgr.GetProgram(szName);

  if (nullptr == program) return kErrFailure;

  GLhandleARB handle = program->GetHandle();

  if (handle != 0) {
    m_pFuncShaders->glDeleteObject(handle);
  }

  m_progamMgr.DestroyProgram(program->GetProgramName());

  return kErrNone;
}

long GlRenderDevice::SetProgramVector(Program *program, string &param,
                                         const Vector4 &value) {
  if (nullptr == program) return kErrFailure;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return kErrFailure;

  m_pFuncShaders->glUniform4fv(loc, 1, (const GLfloat *)&value);

  return kErrNone;
}

long GlRenderDevice::SetProgramVector(Program *program, string &param,
                                         const Vector3 &value) {
  if (nullptr == program) return kErrFailure;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return kErrFailure;

  m_pFuncShaders->glUniform3fv(loc, 1, (const GLfloat *)&value);
  return kErrNone;
}

long GlRenderDevice::SetProgramVector(Program *program, string &param,
                                         const Vector2 &value) {
  if (nullptr == program) return kErrFailure;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return kErrFailure;

  m_pFuncShaders->glUniform2fv(loc, 1, (const GLfloat *)&value);

  return kErrNone;
}

long GlRenderDevice::SetProgramFloat(Program *program, string &param,
                                        float value) {
  if (nullptr == program) return kErrFailure;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return kErrFailure;

  m_pFuncShaders->glUniform1fv(loc, 1, (const GLfloat *)&value);

  return kErrNone;
}

long GlRenderDevice::SetProgramInt(Program *program, string &param,
                                      int value) {
  if (nullptr == program) return kErrFailure;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return kErrFailure;

  m_pFuncShaders->glUniform1i(loc, value);

  return kErrNone;
}

long GlRenderDevice::GetProgramFloat(Program *program, string &param,
                                        float *value) {
  if (nullptr == program) return kErrFailure;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return kErrFailure;

  m_pFuncShaders->glGetUniformfv(handle, loc, value);

  return kErrNone;
}

long GlRenderDevice::SetProgramTexture(Program *program, string &param,
                                          int texture) {
  if (nullptr == program) return kErrFailure;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return kErrFailure;

  m_pFuncShaders->glUniform1i(loc, texture);

  return kErrNone;
}
}  // namespace detail
}  // namespace scenic