// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/impl/gl/host/render_device.h"

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reson_for_call,
                      LPVOID lpReserved) {
  switch (ul_reson_for_call) {
    case DLL_PROCESS_ATTACH:
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      break;
  }
  return true;
}

extern "C" {

LEGACY_RENDER_GL_EXPORT HRESULT
Create3DRenderDevice(HINSTANCE hDLL, render::Smt3DRenderDevice*& pDevice) {
  if (!pDevice) {
    pDevice = new render::SmtGLRenderDevice(hDLL);
    return SMT_OK;
  }

  return SMT_FALSE;
}

LEGACY_RENDER_GL_EXPORT HRESULT
Release3DRenderDevice(render::Smt3DRenderDevice*& pDevice) {
  if (!pDevice) {
    return SMT_FALSE;
  }
  delete pDevice;
  pDevice = nullptr;

  return SMT_OK;
}

}  // extern "C"
