// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/public/device/renderer.h"

#include "scenic/render/backend_dll.h"

namespace scenic {
namespace detail {
namespace {

constexpr BackendDllSpec k_specs[] = {
    {"GdiRenderDevice", "scenic_rhi2d_gdi", "CreateRenderDevice"},
    {"GdiSimpleRenderDevice", "scenic_rhi2d_gdi", "CreateRenderDevice"},
    {"GdiPlusRenderDevice", "scenic_rhi2d_gdiplus", "CreateRenderDevice"},
    {"SkiaRenderDevice", "scenic_rhi2d_skia", "CreateRenderDevice"},
};

}  // namespace

Renderer2d::Renderer2d(HINSTANCE hInst)
    : m_pDevice(nullptr), m_hInst(hInst), m_hDLL(nullptr) {}

Renderer2d::~Renderer2d(void) { Release(); }

int Renderer2d::CreateDevice(const char* chAPI) {
  const BackendDllSpec* spec = find_backend_spec(k_specs, chAPI);
  if (!spec) {
    LOGGING(LOG_ERROR, "API '%s' not yet supported.", chAPI ? chAPI : "");
    return S_FALSE;
  }

  Release();

  m_hDLL = load_backend_dll(spec->dll_stem);
  if (!m_hDLL) {
    return kErrFailure;
  }

  using CreateFn = HRESULT (*)(HINSTANCE, LPRENDERDEVICE&);
  auto* create_fn = reinterpret_cast<CreateFn>(
      ::GetProcAddress(m_hDLL, spec->create_export));
  if (!create_fn) {
    LOGGING(LOG_ERROR, "%s export missing from backend DLL.",
            spec->create_export);
    unload_backend_dll(&m_hDLL);
    return kErrFailure;
  }

  HRESULT hr = create_fn(m_hDLL, m_pDevice);
  if (FAILED(hr) || !m_pDevice) {
    LOGGING(LOG_ERROR, "%s() from lib failed (hr=0x%08lx).",
            spec->create_export, static_cast<unsigned long>(hr));
    m_pDevice = nullptr;
    unload_backend_dll(&m_hDLL);
    return kErrFailure;
  }

  return kErrNone;
}

void Renderer2d::Release(void) {
  if (m_hDLL) {
    using DestroyFn = HRESULT (*)(LPRENDERDEVICE&);
    auto* release_fn = reinterpret_cast<DestroyFn>(
        ::GetProcAddress(m_hDLL, "DestroyRenderDevice"));
    if (m_pDevice && release_fn) {
      HRESULT hr = release_fn(m_pDevice);
      if (FAILED(hr)) {
        LOGGING(LOG_ERROR, "DestroyRenderDevice failed (hr=0x%08lx).",
                static_cast<unsigned long>(hr));
      }
    }
    m_pDevice = nullptr;
    unload_backend_dll(&m_hDLL);
    return;
  }
  m_pDevice = nullptr;
}
}  // namespace detail
}  // namespace scenic
