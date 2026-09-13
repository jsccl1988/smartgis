// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WINUI_DETAIL_LOCAL_RENDER_H_
#define APP_WINUI_DETAIL_LOCAL_RENDER_H_

// FALLBACK ONLY. Chrome must not #include rd_renderdevice.h / gis_map.h.
// When SmartGisRender.exe is absent, probe leftover render dll_stems via
// LoadLibrary (legacy_render / render_gdi* / render_gl; debug → *_d.dll).
// This is not the OOP render path and must not call CreateRenderDevice::Init.

#include <windows.h>

namespace app {
namespace winui {
namespace detail {

struct LocalRenderProbe {
  bool smt_render;
  bool smt_gl;
  bool smt_gdi;
  bool smt_gdi_simple;

  LocalRenderProbe()
      : smt_render(false),
        smt_gl(false),
        smt_gdi(false),
        smt_gdi_simple(false) {}
};

LocalRenderProbe probe_legacy_render_dlls();

const wchar_t* local_render_status_text(const LocalRenderProbe& probe);

}  // namespace detail
}  // namespace winui
}  // namespace app

#endif  // APP_WINUI_DETAIL_LOCAL_RENDER_H_
