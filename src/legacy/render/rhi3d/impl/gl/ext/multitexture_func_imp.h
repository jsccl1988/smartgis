// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MULTITEXTURE_FUNCSIMP_H
#define _MULTITEXTURE_FUNCSIMP_H

#include "legacy/render/rhi3d/impl/gl/ext/multitexture_func.h"
#include "legacy/render/rhi3d/impl/gl/prerequisites.h"

namespace render {
class SmtMultitextureFuncImpl : public SmtMultitextureFunc {
 public:
  SmtMultitextureFuncImpl();
  virtual ~SmtMultitextureFuncImpl();

 public:
  virtual long Initialize(LPGLRENDERDEVICE pGLRenderDevice);

 public:
  virtual void glActiveTexture(GLenum texture);

 private:
  PFNGLACTIVETEXTUREPROC _glActiveTexture;
};
}  // namespace render

#endif  //_MULTITEXTURE_FUNCSIMP_H
