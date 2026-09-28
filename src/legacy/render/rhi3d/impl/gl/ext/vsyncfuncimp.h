// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _VSYNC_FUNCSIMP_H
#define _VSYNC_FUNCSIMP_H

#include "legacy/render/rhi3d/impl/gl/ext/vsyncfunc.h"
#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

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
