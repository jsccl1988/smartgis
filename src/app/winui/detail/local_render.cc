// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/detail/local_render.h"

namespace app {
namespace winui {
namespace detail {
namespace {

bool try_load(const wchar_t* name) {
  if (GetModuleHandleW(name)) {
    return true;
  }
  HMODULE mod = LoadLibraryW(name);
  return mod != nullptr;
}

}  // namespace

LocalRenderProbe probe_legacy_render_dlls() {
  LocalRenderProbe p;
  p.smt_render =
      try_load(L"SmtRenderD.dll") || try_load(L"SmtRender.dll");
  p.smt_gl = try_load(L"SmtGLRenderDeviceD.dll") ||
             try_load(L"SmtGLRenderDevice.dll");
  p.smt_gdi = try_load(L"SmtGdiRenderDeviceD.dll") ||
              try_load(L"SmtGdiRenderDevice.dll");
  p.smt_gdi_simple = try_load(L"SmtGdiSimpleRenderDeviceD.dll") ||
                     try_load(L"SmtGdiSimpleRenderDevice.dll");
  return p;
}

const wchar_t* local_render_status_text(const LocalRenderProbe& probe) {
  if (probe.smt_gl || probe.smt_gdi || probe.smt_gdi_simple ||
      probe.smt_render) {
    return L"FALLBACK: Smt* DLLs loaded (LoadLibrary probe; Init stays "
           L"out of chrome)";
  }
  return L"FALLBACK: SmartGisRender.exe missing; Smt* DLLs not found";
}

}  // namespace detail
}  // namespace winui
}  // namespace app
