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

bool try_stem(const wchar_t* stem_d, const wchar_t* stem) {
  return try_load(stem_d) || try_load(stem);
}

}  // namespace

LocalRenderProbe probe_legacy_render_dlls() {
  LocalRenderProbe p;
  // dll_stem map: all leftover engines → legacy_render.
  p.smt_render =
      try_stem(L"legacy_render_d.dll", L"legacy_render.dll");
  p.smt_gl = p.smt_render;
  p.smt_gdi = p.smt_render;
  p.smt_gdi_simple = p.smt_render;
  return p;
}

const wchar_t* local_render_status_text(const LocalRenderProbe& probe) {
  if (probe.smt_gl || probe.smt_gdi || probe.smt_gdi_simple ||
      probe.smt_render) {
    return L"FALLBACK: leftover render DLLs loaded (LoadLibrary probe; Init "
           L"stays out of chrome)";
  }
  return L"FALLBACK: SmartGisRender.exe missing; leftover render DLLs not "
         L"found";
}

}  // namespace detail
}  // namespace winui
}  // namespace app
