#include "legacy/render/bridge/renderer.h"

namespace render {
SmtRenderer::SmtRenderer(HINSTANCE hInst) {
  m_hInst = hInst;
  m_hDLL = NULL;
  m_pDevice = NULL;
}

SmtRenderer::~SmtRenderer(void) { Release(); }

LPRENDERDEVICE SmtRenderer::GetDevice(void) { return m_pDevice; }

namespace {
HMODULE load_legacy_render_dll() {
#ifdef _DEBUG
  HMODULE dll = LoadLibrary("legacy_render_d.dll");
  if (!dll) {
    ::MessageBox(NULL, "Loading legacy_render_d.dll failed.", "SmartGis - error",
                 MB_OK | MB_ICONERROR);
  }
#else
  HMODULE dll = LoadLibrary("legacy_render.dll");
  if (!dll) {
    ::MessageBox(NULL, "Loading legacy_render.dll failed.", "SmartGis - error",
                 MB_OK | MB_ICONERROR);
  }
#endif
  return dll;
}
}  // namespace

int SmtRenderer::CreateDevice(const char *chAPI) {
  char buffer[300];
  const bool simple = (strcmp(chAPI, "SmtGdiSimpleRenderDevice") == 0);
  const bool gdi = (strcmp(chAPI, "SmtGdiRenderDevice") == 0);

  if (!simple && !gdi) {
    _snprintf(buffer, 300, "API '%s' not yet supported.", chAPI);
    ::MessageBox(NULL, buffer, "SmartGis - error", MB_OK | MB_ICONERROR);
    return SMT_FALSE;
  }

  m_hDLL = load_legacy_render_dll();
  if (!m_hDLL) {
    return SMT_ERR_FAILURE;
  }

  // gdi keeps CreateRenderDevice; simple uses a distinct export after DLL merge.
  const char* create_name =
      simple ? "CreateGdiSimpleRenderDevice" : "CreateRenderDevice";
  destroy_name_ = simple ? "DestroyGdiSimpleRenderDevice" : "DestroyRenderDevice";

  auto* create_fn =
      reinterpret_cast<_CreateRenderDevice>(GetProcAddress(m_hDLL, create_name));
  if (!create_fn) {
    return SMT_ERR_FAILURE;
  }

  HRESULT hr = create_fn(m_hDLL, m_pDevice);
  if (FAILED(hr)) {
    ::MessageBox(NULL, "CreateRenderDevice() from lib failed.", "SmtGis - error",
                 MB_OK | MB_ICONERROR);
    m_pDevice = NULL;
    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

void SmtRenderer::Release(void) {
  _DestroyRenderDevice release_fn = 0;

  if (m_hDLL && destroy_name_) {
    release_fn =
        (_DestroyRenderDevice)GetProcAddress(m_hDLL, destroy_name_);
  }

  if (m_pDevice && release_fn) {
    release_fn(m_pDevice);
  }
  // DestroyRenderDevice nulls its arg; clear local alias to prevent double-free
  // if Release runs again from the destructor after EndDestory.
  m_pDevice = nullptr;
  destroy_name_ = nullptr;
}
}  // namespace render
