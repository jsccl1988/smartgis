// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

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

SCENIC_RENDER_GL_EXPORT HRESULT
Create3DRenderDevice(HINSTANCE hDLL, scenic::detail::RenderDevice3d*& pDevice) {
  if (!pDevice) {
    pDevice = new scenic::detail::GlRenderDevice(hDLL);
    return S_OK;
  }

  return S_FALSE;
}

SCENIC_RENDER_GL_EXPORT HRESULT
Release3DRenderDevice(scenic::detail::RenderDevice3d*& pDevice) {
  if (!pDevice) {
    return S_FALSE;
  }
  delete pDevice;
  pDevice = nullptr;

  return S_OK;
}

}  // extern "C"
