// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MIPMAP_FUNCSIMP_H
#define _MIPMAP_FUNCSIMP_H

#include "legacy/render/rhi3d/impl/gl/ext/mipmap_func.h"
#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

namespace render {
class SmtMipmapFuncImpl : public SmtMipmapFunc {
 public:
  SmtMipmapFuncImpl();
  virtual ~SmtMipmapFuncImpl();

 public:
  virtual long Initialize(LPGLRENDERDEVICE pGLRenderDevice);

 public:
  virtual void glGenerateMipmap(GLenum target);

 private:
  PFNGLGENERATEMIPMAPEXTPROC _glGenerateMipmap;
};
}  // namespace render

#endif  //_VSYNC_FUNCSIMP_H
