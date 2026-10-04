// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/public/device/renderer.h"

#include <cstring>

#include "base/core/log.h"

namespace scenic {
namespace detail {
namespace {

struct DeviceDllSpec {
  const char* api;
  const char* dll_stem;
  const char* create_export;
};

constexpr DeviceDllSpec k_specs[] = {
    {"GdiRenderDevice", "scenic_rhi2d_gdi", "CreateRenderDevice"},
    {"GdiSimpleRenderDevice", "scenic_rhi2d_gdi", "CreateRenderDevice"},
    {"GdiPlusRenderDevice", "scenic_rhi2d_gdiplus", "CreateRenderDevice"},
    {"SkiaRenderDevice", "scenic_rhi2d_skia", "CreateRenderDevice"},
};

const DeviceDllSpec* find_spec(const char* api) {
  if (!api) {
    return nullptr;
  }
  for (const DeviceDllSpec& s : k_specs) {
    if (std::strcmp(api, s.api) == 0) {
      return &s;
    }
  }
  return nullptr;
}

HMODULE load_backend_dll(const char* stem) {
  char name[64];
#ifdef _DEBUG
  _snprintf(name, sizeof(name), "%s_d.dll", stem);
#else
  _snprintf(name, sizeof(name), "%s.dll", stem);
#endif
  HMODULE dll = ::LoadLibraryA(name);
  if (!dll) {
    LOGGING(LOG_ERROR, "Loading %s failed (GetLastError=%lu).", name,
            static_cast<unsigned long>(::GetLastError()));
  }
  return dll;
}

void unload_backend_dll(HMODULE* dll) {
  if (dll && *dll) {
    ::FreeLibrary(*dll);
    *dll = nullptr;
  }
}

}  // namespace

Renderer2d::Renderer2d(HINSTANCE hInst)
    : m_pDevice(nullptr), m_hInst(hInst), m_hDLL(nullptr) {}

Renderer2d::~Renderer2d(void) { Release(); }

int Renderer2d::CreateDevice(const char* chAPI) {
  const DeviceDllSpec* spec = find_spec(chAPI);
  if (!spec) {
    LOGGING(LOG_ERROR, "API '%s' not yet supported.", chAPI ? chAPI : "");
    return SMT_FALSE;
  }

  // Replace any prior backend before loading a new one.
  Release();

  m_hDLL = load_backend_dll(spec->dll_stem);
  if (!m_hDLL) {
    return SMT_ERR_FAILURE;
  }

  using CreateFn = HRESULT (*)(HINSTANCE, LPRENDERDEVICE&);
  auto* create_fn = reinterpret_cast<CreateFn>(
      ::GetProcAddress(m_hDLL, spec->create_export));
  if (!create_fn) {
    LOGGING(LOG_ERROR, "%s export missing from backend DLL.",
            spec->create_export);
    unload_backend_dll(&m_hDLL);
    return SMT_ERR_FAILURE;
  }

  HRESULT hr = create_fn(m_hDLL, m_pDevice);
  if (FAILED(hr) || !m_pDevice) {
    LOGGING(LOG_ERROR, "%s() from lib failed (hr=0x%08lx).",
            spec->create_export, static_cast<unsigned long>(hr));
    m_pDevice = nullptr;
    unload_backend_dll(&m_hDLL);
    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
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
    // DestroyRenderDevice nulls its arg; clear local alias to prevent
    // double-free if Release runs again from the destructor.
    m_pDevice = nullptr;
    unload_backend_dll(&m_hDLL);
    return;
  }
  m_pDevice = nullptr;
}
}  // namespace detail
}  // namespace scenic
