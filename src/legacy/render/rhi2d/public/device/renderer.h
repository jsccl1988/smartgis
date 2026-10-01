// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef _RD_RENDERER_H
#define _RD_RENDERER_H

#include "legacy/core/macros/macros.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"

namespace render {
// Loads per-API leftover 2D backend DLLs (legacy_rhi2d_gdi / gdiplus / skia)
// and resolves Create*/DestroyRenderDevice via GetProcAddress.
// Failure paths log via LOGGING(LOG_ERROR); they do not show MessageBox.
class LEGACY_RENDER_EXPORT SmtRenderer {
 public:
  explicit SmtRenderer(HINSTANCE hInst);
  ~SmtRenderer();

  int CreateDevice(const char* chAPI);
  LPRENDERDEVICE GetDevice(void) { return m_pDevice; }
  HINSTANCE GetModule(void) { return m_hDLL; }
  void Release(void);

 private:
  LPRENDERDEVICE m_pDevice = nullptr;
  HINSTANCE m_hInst = nullptr;
  HMODULE m_hDLL = nullptr;
};

typedef SmtRenderer* LPRENDERER;
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD_RENDERER_H
