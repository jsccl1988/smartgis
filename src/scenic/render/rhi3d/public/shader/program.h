// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_PROGRAM_H
#define _RD3D_PROGRAM_H

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_defs.h"
#include "scenic/render/rhi3d/public/device/base.h"
#include "scenic/render/rhi3d/public/shader/shader.h"

namespace scenic {
namespace detail {
class RenderDevice3d;
typedef class RenderDevice3d *LP3DRENDERDEVICE;

class SCENIC_IMPL_EXPORT Program {
 public:
  Program(LP3DRENDERDEVICE p3DRenderDevice, uint handle, string strName);
  virtual ~Program();

 public:
  inline uint GetHandle() { return m_unHandle; }
  const char *GetProgramName(void) { return m_strName.c_str(); }

 public:
  long Use();
  long Unuse();

  virtual long SetVertexShader(Shader *shader);
  virtual long SetPixelShader(Shader *shader);

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
}  // namespace detail
}  // namespace scenic

#if !defined(SCENIC_IMPL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD3D_PROGRAM_H

// Bodies call RenderDevice3d. This header is included before that type is
// complete, so the bodies are emitted only from the re-include at the bottom
// of render_device.h.
#if defined(SCENIC_3DRENDERDEVICE_COMPLETE) && !defined(_RD3D_PROGRAM_METHODS)
#define _RD3D_PROGRAM_METHODS

namespace scenic {
namespace detail {

inline Program::Program(LP3DRENDERDEVICE p3DRenderDevice, uint handle,
                              string strName)
    : m_unHandle(handle),
      m_p3DRenderDevice(p3DRenderDevice),
      m_strName(strName) {
  ;
}

inline Program::~Program() { ; }

inline long Program::Use() { return m_p3DRenderDevice->BindProgram(this); }

inline long Program::Unuse() { return m_p3DRenderDevice->UnbindProgram(); }

inline long Program::SetVertexShader(Shader *shader) {
  return m_p3DRenderDevice->SetProgramVertexShader(this, shader);
}

inline long Program::SetPixelShader(Shader *shader) {
  return m_p3DRenderDevice->SetProgramPixelShader(this, shader);
}

inline long Program::Link(ShaderCompilationFlag flags) {
  if (kErrNone != m_p3DRenderDevice->LinkProgram(this))
    return kErrFailure;

  /* Check if there is a need to check compilation */
  if (flags & SCF_CHECK_ERRORS) {
    long result = IsLinked();

    /* Check if there is a need to place errors in log file */
    if (kErrNone != result && (flags & SCF_LOG_ERRORS)) {
      ;
    }

    return result;
  }

  return kErrNone;
}

inline long Program::IsLinked() {
  return m_p3DRenderDevice->IsProgramLinked(this);
}

inline char *Program::GetLinkLog() {
  return m_p3DRenderDevice->GetProgramLinkLog(this);
}

inline long Program::SetFloat(string param, float value) {
  return m_p3DRenderDevice->SetProgramFloat(this, param, value);
}

inline long Program::GetFloat(string param, float *value) {
  return m_p3DRenderDevice->GetProgramFloat(this, param, value);
}

inline long Program::SetVector(string param, const Vector2 &value) {
  return m_p3DRenderDevice->SetProgramVector(this, param, value);
}

inline long Program::SetVector(string param, const Vector3 &value) {
  return m_p3DRenderDevice->SetProgramVector(this, param, value);
}

inline long Program::SetVector(string param, const Vector4 &value) {
  return m_p3DRenderDevice->SetProgramVector(this, param, value);
}

inline long Program::SetTexture(string param, int texture) {
  return m_p3DRenderDevice->SetProgramTexture(this, param, texture);
}

inline long Program::SetInt(string param, int value) {
  return m_p3DRenderDevice->SetProgramInt(this, param, value);
}

}  // namespace detail
}  // namespace scenic

#endif  // _RD3D_PROGRAM_METHODS
