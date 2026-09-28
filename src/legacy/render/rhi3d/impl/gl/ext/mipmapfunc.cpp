#include "legacy/render/rhi3d/impl/gl/ext/mipmapfunc.h"

namespace render {
SmtMipmapFunc::SmtMipmapFunc() {}

SmtMipmapFunc::~SmtMipmapFunc() {}

long SmtMipmapFunc::Initialize(LPGLRENDERDEVICE pGLRenderDevice) {
  return SMT_ERR_NONE;
}

void SmtMipmapFunc::glGenerateMipmap(GLenum target) { ; }
}  // namespace render