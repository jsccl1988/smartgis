// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SCENIC_RENDER_RHI2D_PUBLIC_DEVICE_RENDERER_H_
#define SCENIC_RENDER_RHI2D_PUBLIC_DEVICE_RENDERER_H_

#include "scenic/render/err.h"
#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi2d/public/device/render_device.h"

namespace scenic {
namespace detail {
// Loads per-API leftover 2D backend DLLs (scenic_rhi2d_gdi / gdiplus / skia)
// and resolves Create*/DestroyRenderDevice via GetProcAddress.
// Failure paths log via LOGGING(LOG_ERROR); they do not show MessageBox.
class SCENIC_IMPL_EXPORT Renderer2d {
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

#if !defined(SCENIC_IMPL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  // SCENIC_RENDER_RHI2D_PUBLIC_DEVICE_RENDERER_H_
