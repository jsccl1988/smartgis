// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MIPMAP_FUNCS_H
#define _MIPMAP_FUNCS_H

#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

namespace render {
class SmtGLRenderDevice;
typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

class SmtMipmapFunc {
 public:
  SmtMipmapFunc();
  virtual ~SmtMipmapFunc();
  virtual long Initialize(LPGLRENDERDEVICE pGLRenderDevice);

 public:
  virtual void glGenerateMipmap(GLenum target);
};
}  // namespace render

#endif  //_MIPMAP_FUNCS_H
