/*
File:    rd_renderer.h

Desc:    SmtRenderer,���ص�ͼ��Ⱦ������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD_RENDERER_H
#define _RD_RENDERER_H

#include "base/core/core.h"
#include "render/renderdevice.h"

namespace render {
class RENDER_EXPORT_CLASS SmtRenderer {
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
};

typedef SmtRenderer* LPRENDERER;
}  // namespace render

#if !defined(RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "renderD.lib")
#else
#pragma comment(lib, "render.lib")
#endif
#endif

#endif  //_RD_RENDERER_H