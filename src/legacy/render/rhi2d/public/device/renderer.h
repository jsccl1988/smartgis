// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD_RENDERER_H
#define _RD_RENDERER_H

#include "legacy/core/macros/macros.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"

namespace render {
class LEGACY_RENDER_EXPORT SmtRenderer {
 public:
  SmtRenderer(HINSTANCE hInst);
  ~SmtRenderer(void);

  int CreateDevice(const char* chAPI);
  LPRENDERDEVICE GetDevice(void);
  void Release(void);

 private:
  LPRENDERDEVICE m_pDevice;
  HINSTANCE m_hInst;
  HMODULE m_hDLL;
  const char* destroy_name_ = nullptr;
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