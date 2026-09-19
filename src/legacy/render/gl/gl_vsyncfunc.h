/*
File:    gl_vsyncfunc.h

Desc:

Version: Version 1.0

Writter:  �´���

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _VSYNC_FUNCS_H
#define _VSYNC_FUNCS_H

#include "legacy/render/gl/gl_prerequisites.h"

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
