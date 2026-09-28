// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_RENDERER_H
#define _RD3D_RENDERER_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/3drenderdevice.h"

namespace render {
class LEGACY_RENDER_EXPORT Smt3DRenderer {
 public:
  Smt3DRenderer(HINSTANCE hInst);
  ~Smt3DRenderer(void);

  long CreateDevice(const char *chAPI);
  LP3DRENDERDEVICE GetDevice(void) { return m_pDevice; }
  HINSTANCE GetModule(void) { return m_hDLL; }
  void Release(void);

 private:
  Smt3DRenderDevice *m_pDevice;
  HINSTANCE m_hInst;
  HMODULE m_hDLL;
};

inline Smt3DRenderer::Smt3DRenderer(HINSTANCE hInst) {
  m_hInst = hInst;
  m_pDevice = NULL;
  m_hDLL = NULL;
}

inline Smt3DRenderer::~Smt3DRenderer(void) { Release(); }

inline long Smt3DRenderer::CreateDevice(const char *chAPI) {
  char buffer[300];

  if (strcmp(chAPI, "OpenGL") == 0 || strcmp(chAPI, "Direct3D") == 0) {
#ifdef _DEBUG
    m_hDLL = LoadLibrary("legacy_render_d.dll");
    if (!m_hDLL) {
      ::MessageBox(NULL, "Loading legacy_render_d.dll failed.",
                   "SmartGis - error", MB_OK | MB_ICONERROR);
      return SMT_FALSE;
    }
#else
    m_hDLL = LoadLibrary("legacy_render.dll");
    if (!m_hDLL) {
      ::MessageBox(NULL, "Loading legacy_render.dll failed.",
                   "SmartGis - error", MB_OK | MB_ICONERROR);
      return SMT_FALSE;
    }
#endif
  } else {
    _snprintf(buffer, 300, "API '%s' not yet supported.", chAPI);
    ::MessageBox(NULL, buffer, "SmartGis - error", MB_OK | MB_ICONERROR);
    return SMT_FALSE;
  }

  HRESULT hr;
  if (strcmp(chAPI, "Direct3D") == 0) {
    // D3D11 leftover device (rhi/impl/d3d); export distinct from GL factory.
    typedef HRESULT (*_CreateD3DRenderDevice)(HINSTANCE hDLL,
                                              Smt3DRenderDevice *&pInterface);
    _CreateD3DRenderDevice create_d3d =
        (_CreateD3DRenderDevice)GetProcAddress(m_hDLL, "CreateD3DRenderDevice");
    if (!create_d3d) {
      ::MessageBox(NULL, "CreateD3DRenderDevice() export missing.",
                   "SmartGis - error", MB_OK | MB_ICONERROR);
      return SMT_ERR_FAILURE;
    }
    hr = create_d3d(m_hDLL, m_pDevice);
    if (FAILED(hr)) {
      ::MessageBox(NULL, "CreateD3DRenderDevice() from lib failed.",
                   "SmartGis - error", MB_OK | MB_ICONERROR);
      m_pDevice = NULL;
      return SMT_ERR_FAILURE;
    }
    return SMT_ERR_NONE;
  }

  _Create3DRenderDevice _CreateRDev = 0;
  _CreateRDev =
      (_Create3DRenderDevice)GetProcAddress(m_hDLL, "Create3DRenderDevice");

  hr = _CreateRDev(m_hDLL, m_pDevice);

  if (FAILED(hr)) {
    ::MessageBox(NULL, "Create3DRenderDevice() from lib failed.",
                 "SmartGis - error", MB_OK | MB_ICONERROR);
    m_pDevice = NULL;

    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

inline void Smt3DRenderer::Release(void) {
  _Release3DRenderDevice _ReleaseRDev = 0;
  HRESULT hr;

  if (m_hDLL) {
    _ReleaseRDev =
        (_Release3DRenderDevice)GetProcAddress(m_hDLL, "Release3DRenderDevice");
  }

  if (m_pDevice) {
    hr = _ReleaseRDev(m_pDevice);
    if (FAILED(hr)) {
      m_pDevice = NULL;
    }
  }
}

typedef Smt3DRenderer *LPSMT3DRENDERER;
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD3D_RENDERER_H