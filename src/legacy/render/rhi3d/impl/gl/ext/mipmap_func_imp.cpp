#include "legacy/render/rhi3d/impl/gl/ext/mipmap_func_imp.h"

#include "legacy/render/rhi3d/impl/gl/host/render_device.h"

namespace render {
SmtMipmapFuncImpl::SmtMipmapFuncImpl() {}

SmtMipmapFuncImpl::~SmtMipmapFuncImpl() {}

long SmtMipmapFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice) {
  _glGenerateMipmap =
      (PFNGLGENERATEMIPMAPEXTPROC)pGLRenderDevice->GetProcAddress(
          "glGenerateMipmap");

  if (NULL == _glGenerateMipmap) {
    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

void SmtMipmapFuncImpl::glGenerateMipmap(GLenum target) {
  _glGenerateMipmap(target);
}
}  // namespace render