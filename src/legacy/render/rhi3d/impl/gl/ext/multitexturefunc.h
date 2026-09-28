// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MULTITEXTURE_FUNCS_H
#define _MULTITEXTURE_FUNCS_H

#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

namespace render {
class SmtGLRenderDevice;
typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

class SmtMultitextureFunc {
 public:
  SmtMultitextureFunc();
  virtual ~SmtMultitextureFunc();

 public:
  virtual long Initialize(LPGLRENDERDEVICE pGLRenderDevice);

 public:
  virtual void glActiveTexture(GLenum texture);
};
}  // namespace render

#endif  //_MULTITEXTURE_FUNCS_H
