// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/public/device/renderer.h"

#include <cstring>

#include "base/core/log.h"

namespace render {
namespace {

struct DeviceDllSpec {
  const char* api;
  const char* dll_stem;
  const char* create_export;
};

constexpr DeviceDllSpec k_specs[] = {
    {"OpenGL", "legacy_render_gl", "Create3DRenderDevice"},
    {"Direct3D", "legacy_render_d3d", "CreateD3DRenderDevice"},
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

Smt3DRenderer::Smt3DRenderer(HINSTANCE hInst)
    : m_pDevice(nullptr), m_hInst(hInst), m_hDLL(nullptr) {}

Smt3DRenderer::~Smt3DRenderer(void) { Release(); }

long Smt3DRenderer::CreateDevice(const char* chAPI) {
  const DeviceDllSpec* spec = find_spec(chAPI);
  if (!spec) {
    LOGGING(LOG_ERROR, "API '%s' not yet supported.", chAPI ? chAPI : "");
    return SMT_FALSE;
  }

  // Replace any prior backend before loading a new one.
  Release();

  m_hDLL = load_backend_dll(spec->dll_stem);
  if (!m_hDLL) {
    return SMT_FALSE;
  }

  using CreateFn = HRESULT (*)(HINSTANCE, Smt3DRenderDevice*&);
  auto* create_fn =
      reinterpret_cast<CreateFn>(::GetProcAddress(m_hDLL, spec->create_export));
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

void Smt3DRenderer::Release(void) {
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
}  // namespace render
