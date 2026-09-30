// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_SHADER_H
#define _RD3D_SHADER_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_defs.h"

namespace render {
/**
Defines additional actions for shader compilation and program linking process.
*/
enum ShaderCompilationFlag {
  SCF_NOTHING = 0,
  SCF_CHECK_ERRORS = 1,
  SCF_LOG_ERRORS = SCF_CHECK_ERRORS | 2,
};

class Smt3DRenderDevice;
typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

class LEGACY_RENDER_EXPORT SmtShader {
 public:
  SmtShader(LP3DRENDERDEVICE p3DRenderDevice, uint handle, string strName);
  virtual ~SmtShader();

 public:
  inline uint GetHandle() { return m_unHandle; }
  const char *GetShaderName(void) { return m_strName.c_str(); }

 public:
  virtual long Load(string fileName, bool needToCompile = false,
                    ShaderCompilationFlag flags = SCF_NOTHING);
  virtual long Compile(ShaderCompilationFlag flags = SCF_LOG_ERRORS);
  virtual long IsCompiled();
  virtual char *GetCompileLog();

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

#endif  //_RD3D_SHADER_H

// Bodies call Smt3DRenderDevice. This header is included before that type is
// complete, so the bodies are emitted only from the re-include at the bottom
// of render_device.h.
#if defined(SMT_3DRENDERDEVICE_COMPLETE) && !defined(_RD3D_SHADER_METHODS)
#define _RD3D_SHADER_METHODS

#include <cstdio>
#include <vector>

namespace render {

inline SmtShader::SmtShader(LP3DRENDERDEVICE p3DRenderDevice, uint handle,
                            string strName)
    : m_unHandle(handle),
      m_p3DRenderDevice(p3DRenderDevice),
      m_strName(strName) {
  ;
}

inline SmtShader::~SmtShader() { ; }

inline long SmtShader::Load(std::string fileName, bool needToCompile,
                            ShaderCompilationFlag flags) {
  std::vector<char> data;

  FILE *sFile = fopen(fileName.c_str(), "rb");
  if (NULL == sFile) {
    return SMT_ERR_INVALID_FILE;
  }

  char buf[1024];
  while (!feof(sFile)) {
    int readBytes = fread(buf, 1, sizeof(buf), sFile);
    for (int i = 0; i < readBytes; i++) {
      data.push_back(buf[i]);
    }
  }
  fclose(sFile);

  data.push_back(0);  // To get NULL-terminated string from vector

  if (SMT_ERR_NONE !=
      m_p3DRenderDevice->LoadShaderSource(this, (char *)&data[0]))
    return SMT_ERR_FAILURE;

  if (needToCompile) {
    return Compile(flags);
  }

  return SMT_ERR_NONE;
}

inline long SmtShader::Compile(ShaderCompilationFlag flags) {
  if (SMT_ERR_NONE != m_p3DRenderDevice->CompileShader(this))
    return SMT_ERR_FAILURE;

  /* Check if there is a need to check compilation */
  if (flags & SCF_CHECK_ERRORS) {
    long result = IsCompiled();

    /* Check if there is a need to place errors in log file */
    if (result != SMT_ERR_NONE && (flags & SCF_LOG_ERRORS)) {
      GetCompileLog();
    }

    return result;
  }

  return SMT_ERR_NONE;
}

inline long SmtShader::IsCompiled() {
  return m_p3DRenderDevice->IsShaderCompiled(this);
}

inline char *SmtShader::GetCompileLog() {
  return m_p3DRenderDevice->GetShaderLog(this);
}

}  // namespace render

#endif  // _RD3D_SHADER_METHODS