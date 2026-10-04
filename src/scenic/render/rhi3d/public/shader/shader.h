// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_SHADER_H
#define _RD3D_SHADER_H

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_defs.h"

namespace scenic {
namespace detail {
/**
Defines additional actions for shader compilation and program linking process.
*/
enum ShaderCompilationFlag {
  SCF_NOTHING = 0,
  SCF_CHECK_ERRORS = 1,
  SCF_LOG_ERRORS = SCF_CHECK_ERRORS | 2,
};

class RenderDevice3d;
typedef class RenderDevice3d *LP3DRENDERDEVICE;

class SCENIC_IMPL_EXPORT Shader {
 public:
  Shader(LP3DRENDERDEVICE p3DRenderDevice, uint handle, string strName);
  virtual ~Shader();

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
}  // namespace detail
}  // namespace scenic

#if !defined(SCENIC_IMPL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD3D_SHADER_H

// Bodies call RenderDevice3d. This header is included before that type is
// complete, so the bodies are emitted only from the re-include at the bottom
// of render_device.h.
#if defined(SCENIC_3DRENDERDEVICE_COMPLETE) && !defined(_RD3D_SHADER_METHODS)
#define _RD3D_SHADER_METHODS

#include <cstdio>
#include <vector>

namespace scenic {
namespace detail {

inline Shader::Shader(LP3DRENDERDEVICE p3DRenderDevice, uint handle,
                            string strName)
    : m_unHandle(handle),
      m_p3DRenderDevice(p3DRenderDevice),
      m_strName(strName) {
  ;
}

inline Shader::~Shader() { ; }

inline long Shader::Load(std::string fileName, bool needToCompile,
                            ShaderCompilationFlag flags) {
  std::vector<char> data;

  FILE *sFile = fopen(fileName.c_str(), "rb");
  if (nullptr == sFile) {
    return kErrInvalidFile;
  }

  char buf[1024];
  while (!feof(sFile)) {
    int readBytes = fread(buf, 1, sizeof(buf), sFile);
    for (int i = 0; i < readBytes; i++) {
      data.push_back(buf[i]);
    }
  }
  fclose(sFile);

  data.push_back(0);  // To get nullptr-terminated string from vector

  if (kErrNone !=
      m_p3DRenderDevice->LoadShaderSource(this, (char *)&data[0]))
    return kErrFailure;

  if (needToCompile) {
    return Compile(flags);
  }

  return kErrNone;
}

inline long Shader::Compile(ShaderCompilationFlag flags) {
  if (kErrNone != m_p3DRenderDevice->CompileShader(this))
    return kErrFailure;

  /* Check if there is a need to check compilation */
  if (flags & SCF_CHECK_ERRORS) {
    long result = IsCompiled();

    /* Check if there is a need to place errors in log file */
    if (result != kErrNone && (flags & SCF_LOG_ERRORS)) {
      GetCompileLog();
    }

    return result;
  }

  return kErrNone;
}

inline long Shader::IsCompiled() {
  return m_p3DRenderDevice->IsShaderCompiled(this);
}

inline char *Shader::GetCompileLog() {
  return m_p3DRenderDevice->GetShaderLog(this);
}

}  // namespace detail
}  // namespace scenic

#endif  // _RD3D_SHADER_METHODS