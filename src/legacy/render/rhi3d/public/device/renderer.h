// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef _RD3D_RENDERER_H
#define _RD3D_RENDERER_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_device.h"

namespace render {
// Loads per-API leftover 3D backend DLLs (legacy_render_gl / legacy_render_d3d)
// and resolves Create*/Release3DRenderDevice via GetProcAddress.
// Failure paths log via LOGGING(LOG_ERROR); they do not show MessageBox.
class LEGACY_RENDER_EXPORT Smt3DRenderer {
 public:
  explicit Smt3DRenderer(HINSTANCE hInst);
  ~Smt3DRenderer();

  long CreateDevice(const char* chAPI);
  LP3DRENDERDEVICE GetDevice(void) { return m_pDevice; }
  HINSTANCE GetModule(void) { return m_hDLL; }
  void Release(void);

 private:
  Smt3DRenderDevice* m_pDevice = nullptr;
  HINSTANCE m_hInst = nullptr;
  HMODULE m_hDLL = nullptr;
};

typedef Smt3DRenderer* LPSMT3DRENDERER;
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD3D_RENDERER_H
