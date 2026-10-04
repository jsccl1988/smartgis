// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef _RD_RENDERER_H
#define _RD_RENDERER_H

#include "scenic/detail/err.h"
#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi2d/public/device/renderdevice.h"

namespace scenic {
namespace detail {
// Loads per-API leftover 2D backend DLLs (scenic_rhi2d_gdi / gdiplus / skia)
// and resolves Create*/DestroyRenderDevice via GetProcAddress.
// Failure paths log via LOGGING(LOG_ERROR); they do not show MessageBox.
class LEGACY_RENDER_EXPORT Renderer2d {
 public:
  explicit Renderer2d(HINSTANCE hInst);
  ~Renderer2d();

  int CreateDevice(const char* chAPI);
  LPRENDERDEVICE GetDevice(void) { return m_pDevice; }
  HINSTANCE GetModule(void) { return m_hDLL; }
  void Release(void);

 private:
  LPRENDERDEVICE m_pDevice = nullptr;
  HINSTANCE m_hInst = nullptr;
  HMODULE m_hDLL = nullptr;
};

typedef Renderer2d* LPRENDERER;
}  // namespace detail
}  // namespace scenic

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD_RENDERER_H
