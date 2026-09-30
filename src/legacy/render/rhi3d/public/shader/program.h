// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_PROGRAM_H
#define _RD3D_PROGRAM_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_defs.h"
#include "legacy/render/rhi3d/public/device/base.h"
#include "legacy/render/rhi3d/public/shader/shader.h"

namespace render {
class Smt3DRenderDevice;
typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

class LEGACY_RENDER_EXPORT SmtProgram {
 public:
  SmtProgram(LP3DRENDERDEVICE p3DRenderDevice, uint handle, string strName);
  virtual ~SmtProgram();

 public:
  inline uint GetHandle() { return m_unHandle; }
  const char *GetProgramName(void) { return m_strName.c_str(); }

 public:
  long Use();
  long Unuse();

  virtual long SetVertexShader(SmtShader *shader);
  virtual long SetPixelShader(SmtShader *shader);

  virtual long Link(ShaderCompilationFlag flags = SCF_LOG_ERRORS);
  virtual long IsLinked();
  virtual char *GetLinkLog();

  virtual long SetVector(string param, const Vector4 &value);
  virtual long SetVector(string param, const Vector3 &value);
  virtual long SetVector(string param, const Vector2 &value);
  virtual long SetFloat(string param, float value);
  virtual long SetInt(string param, int value);
  virtual long GetFloat(string param, float *value);
  virtual long SetTexture(string param, int texture);

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  uint m_unHandle;
  string m_strName;
};
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD3D_PROGRAM_H

// Bodies call Smt3DRenderDevice. This header is included before that type is
// complete, so the bodies are emitted only from the re-include at the bottom
// of render_device.h.
#if defined(SMT_3DRENDERDEVICE_COMPLETE) && !defined(_RD3D_PROGRAM_METHODS)
#define _RD3D_PROGRAM_METHODS

namespace render {

inline SmtProgram::SmtProgram(LP3DRENDERDEVICE p3DRenderDevice, uint handle,
                              string strName)
    : m_unHandle(handle),
      m_p3DRenderDevice(p3DRenderDevice),
      m_strName(strName) {
  ;
}

inline SmtProgram::~SmtProgram() { ; }

inline long SmtProgram::Use() { return m_p3DRenderDevice->BindProgram(this); }

inline long SmtProgram::Unuse() { return m_p3DRenderDevice->UnbindProgram(); }

inline long SmtProgram::SetVertexShader(SmtShader *shader) {
  return m_p3DRenderDevice->SetProgramVertexShader(this, shader);
}

inline long SmtProgram::SetPixelShader(SmtShader *shader) {
  return m_p3DRenderDevice->SetProgramPixelShader(this, shader);
}

inline long SmtProgram::Link(ShaderCompilationFlag flags) {
  if (SMT_ERR_NONE != m_p3DRenderDevice->LinkProgram(this))
    return SMT_ERR_FAILURE;

  /* Check if there is a need to check compilation */
  if (flags & SCF_CHECK_ERRORS) {
    long result = IsLinked();

    /* Check if there is a need to place errors in log file */
    if (SMT_ERR_NONE != result && (flags & SCF_LOG_ERRORS)) {
      ;
    }

    return result;
  }

  return SMT_ERR_NONE;
}

inline long SmtProgram::IsLinked() {
  return m_p3DRenderDevice->IsProgramLinked(this);
}

inline char *SmtProgram::GetLinkLog() {
  return m_p3DRenderDevice->GetProgramLinkLog(this);
}

inline long SmtProgram::SetFloat(string param, float value) {
  return m_p3DRenderDevice->SetProgramFloat(this, param, value);
}

inline long SmtProgram::GetFloat(string param, float *value) {
  return m_p3DRenderDevice->GetProgramFloat(this, param, value);
}

inline long SmtProgram::SetVector(string param, const Vector2 &value) {
  return m_p3DRenderDevice->SetProgramVector(this, param, value);
}

inline long SmtProgram::SetVector(string param, const Vector3 &value) {
  return m_p3DRenderDevice->SetProgramVector(this, param, value);
}

inline long SmtProgram::SetVector(string param, const Vector4 &value) {
  return m_p3DRenderDevice->SetProgramVector(this, param, value);
}

inline long SmtProgram::SetTexture(string param, int texture) {
  return m_p3DRenderDevice->SetProgramTexture(this, param, texture);
}

inline long SmtProgram::SetInt(string param, int value) {
  return m_p3DRenderDevice->SetProgramInt(this, param, value);
}

}  // namespace render

#endif  // _RD3D_PROGRAM_METHODS
