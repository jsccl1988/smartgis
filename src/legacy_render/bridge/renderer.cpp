#include "legacy_render/bridge/renderer.h"

namespace render {
SmtRenderer::SmtRenderer(HINSTANCE hInst) {
  m_hInst = hInst;
  m_hDLL = NULL;
  m_pDevice = NULL;
}

SmtRenderer::~SmtRenderer(void) { Release(); }

LPRENDERDEVICE SmtRenderer::GetDevice(void) { return m_pDevice; }

int SmtRenderer::CreateDevice(const char *chAPI) {
  char buffer[300];

  if (strcmp(chAPI, "SmtGdiSimpleRenderDevice") == 0) {
#ifdef _DEBUG
    m_hDLL = LoadLibrary("render_gdi_simpleD.dll");
    if (!m_hDLL) {
      ::MessageBox(NULL, "Loading render_gdi_simpleD.dll failed.",
                   "SmartGis - error", MB_OK | MB_ICONERROR);
      return SMT_ERR_FAILURE;
    }
#else
    m_hDLL = LoadLibrary("render_gdi_simple.dll");
    if (!m_hDLL) {
      ::MessageBox(NULL, "Loading render_gdi_simple.dll failed.",
                   "SmartGis - error", MB_OK | MB_ICONERROR);
      return SMT_ERR_FAILURE;
    }
#endif
  } else if (strcmp(chAPI, "SmtGdiRenderDevice") == 0) {
#ifdef _DEBUG
    m_hDLL = LoadLibrary("render_gdiD.dll");
    if (!m_hDLL) {
      ::MessageBox(NULL, "Loading render_gdiD.dll failed.", "SmartGis - error",
                   MB_OK | MB_ICONERROR);
      return SMT_ERR_FAILURE;
    }
#else
    m_hDLL = LoadLibrary("render_gdi.dll");
    if (!m_hDLL) {
      ::MessageBox(NULL, "Loading render_gdi.dll failed.", "SmartGis - error",
                   MB_OK | MB_ICONERROR);
      return SMT_ERR_FAILURE;
    }
#endif
  } else {
    _snprintf(buffer, 300, "API '%s' not yet supported.", chAPI);
    ::MessageBox(NULL, buffer, "SmartGis - error", MB_OK | MB_ICONERROR);
    return SMT_FALSE;
  }

  _CreateRenderDevice _CreateRenderDev = 0;
  HRESULT hr;

  _CreateRenderDev =
      (_CreateRenderDevice)GetProcAddress(m_hDLL, "CreateRenderDevice");

  if (NULL == _CreateRenderDev) return SMT_ERR_FAILURE;

  hr = _CreateRenderDev(m_hDLL, m_pDevice);

  if (FAILED(hr)) {
    ::MessageBox(NULL, "CreateRenderDevice() from lib failed.",
                 "SmtGis - error", MB_OK | MB_ICONERROR);
    m_pDevice = NULL;

    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

void SmtRenderer::Release(void) {
  _DestroyRenderDevice _ReleaseRenderDev = 0;

  if (m_hDLL) {
    _ReleaseRenderDev =
        (_DestroyRenderDevice)GetProcAddress(m_hDLL, "DestroyRenderDevice");
  }

  if (m_pDevice && _ReleaseRenderDev) {
    _ReleaseRenderDev(m_pDevice);
  }
}
}  // namespace render