// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/public/device/renderer.h"

#include "scenic/render/backend_dll.h"

namespace scenic {
namespace detail {
namespace {

constexpr BackendDllSpec k_specs[] = {
    {"OpenGL", "scenic_render_gl", "Create3DRenderDevice"},
    {"Direct3D", "scenic_render_d3d", "CreateD3DRenderDevice"},
};

}  // namespace

Renderer3d::Renderer3d(HINSTANCE hInst)
    : m_pDevice(nullptr), m_hInst(hInst), m_hDLL(nullptr) {}

Renderer3d::~Renderer3d(void) { Release(); }

long Renderer3d::CreateDevice(const char* chAPI) {
  const BackendDllSpec* spec = find_backend_spec(k_specs, chAPI);
  if (!spec) {
    LOGGING(LOG_ERROR, "API '%s' not yet supported.", chAPI ? chAPI : "");
    return S_FALSE;
  }

  Release();

  m_hDLL = load_backend_dll(spec->dll_stem);
  if (!m_hDLL) {
    return S_FALSE;
  }

  using CreateFn = HRESULT (*)(HINSTANCE, RenderDevice3d*&);
  auto* create_fn =
      reinterpret_cast<CreateFn>(::GetProcAddress(m_hDLL, spec->create_export));
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

void Renderer3d::Release(void) {
  if (m_hDLL) {
    auto* release_fn = reinterpret_cast<_Release3DRenderDevice>(
        ::GetProcAddress(m_hDLL, "Release3DRenderDevice"));
    if (m_pDevice && release_fn) {
      HRESULT hr = release_fn(m_pDevice);
      if (FAILED(hr)) {
        LOGGING(LOG_ERROR, "Release3DRenderDevice failed (hr=0x%08lx).",
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
