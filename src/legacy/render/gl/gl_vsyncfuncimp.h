/*
File:    gl_vsyncfuncimp.h

Desc:

Version: Version 1.0

Writter:  �´���

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _VSYNC_FUNCSIMP_H
#define _VSYNC_FUNCSIMP_H

#include "legacy/render/gl/gl_prerequisites.h"
#include "legacy/render/gl/gl_vsyncfunc.h"

namespace render {
class SmtVSyncFuncImpl : public SmtVSyncFunc {
 public:
  SmtVSyncFuncImpl();
  virtual ~SmtVSyncFuncImpl();

 public:
  virtual long Initialize(LPGLRENDERDEVICE pGLRenderDevice);

 public:
  virtual int WaitForVSync();
  virtual void EnableVSync();
  virtual void DisableVSync();

 private:
  PFNWGLSWAPINTERVALEXTPROC _wglSwapInterval;
};
}  // namespace render

#endif  //_VSYNC_FUNCSIMP_H
