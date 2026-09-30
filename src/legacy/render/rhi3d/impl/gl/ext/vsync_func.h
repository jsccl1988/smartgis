// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _VSYNC_FUNCS_H
#define _VSYNC_FUNCS_H

#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

namespace render {
class SmtGLRenderDevice;
typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

class SmtVSyncFunc {
 public:
  SmtVSyncFunc();
  virtual ~SmtVSyncFunc();
  virtual long Initialize(LPGLRENDERDEVICE pGLRenderDevice);

 public:
  virtual int WaitForVSync();
  virtual void EnableVSync();
  virtual void DisableVSync();
};
}  // namespace render

#endif  //_VSYNC_FUNCS_H
