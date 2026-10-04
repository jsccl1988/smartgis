#include "base/core/log.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"
#include "scenic/render/rhi3d/public/shader/program_manager.h"
#include "scenic/render/rhi3d/public/shader/shader_manager.h"

using namespace base;

namespace scenic {
namespace detail {
// vertex shader
Shader *GlRenderDevice::CreateVertexShader(const char *szName) {
  GLhandleARB newHandle = m_pFuncShaders->glCreateShader(GL_VERTEX_SHADER_ARB);
  Shader *pNewShader = new Shader(this, newHandle, szName);

  if (SMT_ERR_NONE == m_shaderMgr.AddShader(pNewShader))
    return pNewShader;
  else {
    SMT_SAFE_DELETE(pNewShader);
    return nullptr;
  }
}

// pixel shader
Shader *GlRenderDevice::CreatePixelShader(const char *szName) {
  GLhandleARB newHandle =
      m_pFuncShaders->glCreateShader(GL_FRAGMENT_SHADER_ARB);
  Shader *pNewShader = new Shader(this, newHandle, szName);

  if (SMT_ERR_NONE == m_shaderMgr.AddShader(pNewShader))
    return pNewShader;
  else {
    SMT_SAFE_DELETE(pNewShader);
    return nullptr;
  }
}

Shader *GlRenderDevice::GetShader(const char *szName) {
  return m_shaderMgr.GetShader(szName);
}

// program
Program *GlRenderDevice::CreateProgram(const char *szName) {
  GLhandleARB newHandle = m_pFuncShaders->glCreateProgram();
  Program *pNewProgram = new Program(this, newHandle, szName);

  if (SMT_ERR_NONE == m_progamMgr.AddProgram(pNewProgram))
    return pNewProgram;
  else {
    SMT_SAFE_DELETE(pNewProgram);
    return nullptr;
  }
}

Program *GlRenderDevice::GetProgram(const char *szName) {
  return m_progamMgr.GetProgram(szName);
}

long GlRenderDevice::LoadShaderSource(Shader *shader, char *source) {
  GLhandleARB handle = shader->GetHandle();
  m_pFuncShaders->glShaderSource(handle, 1, (const GLchar **)&source, nullptr);

  return SMT_ERR_NONE;
}

long GlRenderDevice::CompileShader(Shader *shader) {
  GLhandleARB handle = shader->GetHandle();
  m_pFuncShaders->glCompileShader(handle);

  return SMT_ERR_NONE;
}

long GlRenderDevice::IsShaderCompiled(Shader *shader) {
  GLhandleARB handle = shader->GetHandle();
  int compileStatus = 0;

  m_pFuncShaders->glGetObjectParameteriv(handle, GL_OBJECT_COMPILE_STATUS_ARB,
                                         &compileStatus);
  if (compileStatus == 0) {
    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
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
  if (nullptr == shader) return SMT_ERR_FAILURE;

  GLhandleARB handle = shader->GetHandle();
  if (handle != 0) {
    m_pFuncShaders->glDeleteObject(handle);
  }

  m_shaderMgr.DestroyShader(shader->GetShaderName());

  return SMT_ERR_NONE;
}

long GlRenderDevice::BindProgram(Program *program) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  m_pFuncShaders->glUseProgram(program->GetHandle());

  return SMT_ERR_NONE;
}

long GlRenderDevice::UnbindProgram() {
  m_pFuncShaders->glUseProgram(0);

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetProgramVertexShader(Program *program,
                                               Shader *shader) {
  if (nullptr == program || nullptr == shader) return SMT_ERR_FAILURE;

  m_pFuncShaders->glAttachShader(program->GetHandle(), shader->GetHandle());

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetProgramPixelShader(Program *program,
                                              Shader *shader) {
  if (nullptr == program || nullptr == shader) return SMT_ERR_FAILURE;

  m_pFuncShaders->glAttachShader(program->GetHandle(), shader->GetHandle());

  return SMT_ERR_NONE;
}

long GlRenderDevice::LinkProgram(Program *program) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  m_pFuncShaders->glLinkProgram(program->GetHandle());

  return SMT_ERR_NONE;
}

long GlRenderDevice::IsProgramLinked(Program *program) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  int linkStatus = 0;

  m_pFuncShaders->glGetObjectParameteriv(
      program->GetHandle(), GL_OBJECT_LINK_STATUS_ARB, &linkStatus);
  if (linkStatus == 0) {
    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
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

  if (nullptr == program) return SMT_ERR_FAILURE;

  GLhandleARB handle = program->GetHandle();

  if (handle != 0) {
    m_pFuncShaders->glDeleteObject(handle);
  }

  m_progamMgr.DestroyProgram(program->GetProgramName());

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetProgramVector(Program *program, string &param,
                                         const Vector4 &value) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return SMT_ERR_FAILURE;

  m_pFuncShaders->glUniform4fv(loc, 1, (const GLfloat *)&value);

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetProgramVector(Program *program, string &param,
                                         const Vector3 &value) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return SMT_ERR_FAILURE;

  m_pFuncShaders->glUniform3fv(loc, 1, (const GLfloat *)&value);
  return SMT_ERR_NONE;
}

long GlRenderDevice::SetProgramVector(Program *program, string &param,
                                         const Vector2 &value) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return SMT_ERR_FAILURE;

  m_pFuncShaders->glUniform2fv(loc, 1, (const GLfloat *)&value);

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetProgramFloat(Program *program, string &param,
                                        float value) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return SMT_ERR_FAILURE;

  m_pFuncShaders->glUniform1fv(loc, 1, (const GLfloat *)&value);

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetProgramInt(Program *program, string &param,
                                      int value) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return SMT_ERR_FAILURE;

  m_pFuncShaders->glUniform1i(loc, value);

  return SMT_ERR_NONE;
}

long GlRenderDevice::GetProgramFloat(Program *program, string &param,
                                        float *value) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return SMT_ERR_FAILURE;

  m_pFuncShaders->glGetUniformfv(handle, loc, value);

  return SMT_ERR_NONE;
}

long GlRenderDevice::SetProgramTexture(Program *program, string &param,
                                          int texture) {
  if (nullptr == program) return SMT_ERR_FAILURE;

  GLhandleARB handle = program->GetHandle();

  int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
  if (loc < 0) return SMT_ERR_FAILURE;

  m_pFuncShaders->glUniform1i(loc, texture);

  return SMT_ERR_NONE;
}
}  // namespace detail
}  // namespace scenic