#include "legacy/render/rhi3d/impl/gl/ext/multitexture_func_imp.h"

#include "legacy/render/rhi3d/impl/gl/host/render_device.h"

namespace render {
SmtMultitextureFuncImpl::SmtMultitextureFuncImpl() {}

SmtMultitextureFuncImpl::~SmtMultitextureFuncImpl() {}

long SmtMultitextureFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice) {
  _glActiveTexture = (PFNGLACTIVETEXTUREPROC)pGLRenderDevice->GetProcAddress(
      "glActiveTextureARB");

  if (NULL == _glActiveTexture) {
    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

void SmtMultitextureFuncImpl::glActiveTexture(GLenum texture) {
  _glActiveTexture(texture);
}
}  // namespace render