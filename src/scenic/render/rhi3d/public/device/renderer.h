// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef _RD3D_RENDERER_H
#define _RD3D_RENDERER_H

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"

namespace scenic {
namespace detail {
// Loads per-API leftover 3D backend DLLs (scenic_render_gl / scenic_render_d3d)
// and resolves Create*/Release3DRenderDevice via GetProcAddress.
// Failure paths log via LOGGING(LOG_ERROR); they do not show MessageBox.
class SCENIC_IMPL_EXPORT Renderer3d {
 public:
  explicit Renderer3d(HINSTANCE hInst);
  ~Renderer3d();

  long CreateDevice(const char* chAPI);
  LP3DRENDERDEVICE GetDevice(void) { return m_pDevice; }
  HINSTANCE GetModule(void) { return m_hDLL; }
  void Release(void);

 private:
  RenderDevice3d* m_pDevice = nullptr;
  HINSTANCE m_hInst = nullptr;
  HMODULE m_hDLL = nullptr;
};

}  // namespace detail
}  // namespace scenic

#if !defined(SCENIC_IMPL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD3D_RENDERER_H
