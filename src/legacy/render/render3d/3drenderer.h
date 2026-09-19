/*
File:    rd3d_3drenderer.h

Desc:    Smt3DRenderer,���ص�ͼ��Ⱦ������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _RD3D_RENDERER_H
#define _RD3D_RENDERER_H
#include "legacy/render/render3d/3drenderdevice.h"

namespace render {
class RENDER3D_EXPORT_CLASS Smt3DRenderer {
 public:
  Smt3DRenderer(HINSTANCE hInst);
  ~Smt3DRenderer(void);

  long CreateDevice(const char *chAPI);
  LP3DRENDERDEVICE GetDevice(void) { return m_pDevice; }
  HINSTANCE GetModule(void) { return m_hDLL; }
  void Release(void);

 private:
  Smt3DRenderDevice *m_pDevice;
  HINSTANCE m_hInst;
  HMODULE m_hDLL;
};

typedef Smt3DRenderer *LPSMT3DRENDERER;
}  // namespace render

#if !defined(RENDER3D_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD3D_RENDERER_H