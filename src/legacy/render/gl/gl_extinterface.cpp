#include "legacy/render/gl/gl_3drenderdevice.h"

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

RENDER_GL_EXPORT_API HRESULT
Create3DRenderDevice(HINSTANCE hDLL, render::Smt3DRenderDevice*& pDevice) {
  if (!pDevice) {
    pDevice = new render::SmtGLRenderDevice(hDLL);
    return SMT_OK;
  }

  return SMT_FALSE;
}

RENDER_GL_EXPORT_API HRESULT
Release3DRenderDevice(render::Smt3DRenderDevice*& pDevice) {
  if (!pDevice) {
    return SMT_FALSE;
  }
  delete pDevice;
  pDevice = NULL;

  return SMT_OK;
}

}  // extern "C"