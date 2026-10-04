#include "base/core/log.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"
using namespace base;

namespace scenic {
namespace detail {
Texture *GlRenderDevice::CreateTexture(const char *szName) {
  GLuint unHandle;
  glGenTextures(1, &unHandle);

  Texture *pTex = new Texture(this, unHandle, szName);
  if (kErrNone == m_textureMgr.AddTexture(pTex))
    return pTex;
  else {
    SAFE_DELETE(pTex);
    return nullptr;
  }
}

long GlRenderDevice::GenerateMipmap(Texture *pTexture) {
  if (nullptr == pTexture) return kErrInvalidParam;

  if (kErrNone == pTexture->Use()) {
    m_pFuncMipmap->glGenerateMipmap(GL_TEXTURE_2D);
    pTexture->Unuse();

    return kErrNone;
  }

  return kErrFailure;
}

long GlRenderDevice::DestroyTexture(const char *szName) {
  Texture *pTexture = m_textureMgr.GetTexture(szName);

  if (nullptr == pTexture) return kErrInvalidParam;

  GLhandleARB handle = pTexture->GetHandle();
  if (handle != 0) {
    glDeleteTextures(1, &handle);
  }

  m_textureMgr.DestroyTexture(pTexture->GetTextureName());

  return kErrNone;
}

Texture *GlRenderDevice::GetTexture(const char *szName) {
  return m_textureMgr.GetTexture(szName);
}

long GlRenderDevice::BindTexture(Texture *pTexture) {
  if (nullptr == pTexture) return kErrInvalidParam;

  GLhandleARB handle = pTexture->GetHandle();
  glBindTexture(GL_TEXTURE_2D, handle);

  return kErrNone;
}

long GlRenderDevice::BuildTexture(Texture *pTexture) {
  if (nullptr == pTexture) return kErrInvalidParam;

  void *pDataBuf = pTexture->GetData();

  if (nullptr == pDataBuf) return kErrInvalidParam;

  TextureDesc texDesc = pTexture->GetDesc();

  GLuint internalFormat = ConvertTexFormat(texDesc.format);
  GLuint nativeFormat = 0;
  int components = pTexture->GetComponents(texDesc.format);

  GLenum target = GL_TEXTURE_2D;

  bool bPowerOfTwo = pTexture->IsSizePowerOfTwo();

  /*if (!bPowerOfTwo)
  target = GL_TEXTURE_RECTANGLE_ARB;*/

  if (components == 1)
    nativeFormat = GL_LUMINANCE;
  else {
    if (components == 3)
      nativeFormat = GL_RGB;
    else
      nativeFormat = GL_RGBA;
  }

  TextureSampler texSampler;
  TextureEnvMode texEvn;
  texSampler.rTexture = REPEAT;
  texSampler.sTexture = REPEAT;
  texSampler.tTexture = REPEAT;
  texEvn.envMode = MODULATE;

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glBindTexture(target, pTexture->GetHandle());

  if (pTexture->IsUseMipmap()) {
    texSampler.magFilter = LINEAR_MIPMAP_LINEAR;
    texSampler.minFilter = LINEAR_MIPMAP_LINEAR;

    gluBuild2DMipmaps(target, components, texDesc.width, texDesc.height,
                      nativeFormat, GL_UNSIGNED_BYTE, pDataBuf);

    // m_p3DRenderDevice->GenerateMipmap(this);
  } else {
    texSampler.magFilter = LINEAR;
    texSampler.minFilter = LINEAR;

    glTexImage2D(target, 0, internalFormat, texDesc.width, texDesc.height, 0,
                 nativeFormat, GL_UNSIGNED_BYTE, pDataBuf);
  }

  pTexture->SetEnvMode(texEvn);
  pTexture->SetSampler(texSampler);

  return kErrNone;
}

long GlRenderDevice::UnbindTexture() {
  glBindTexture(GL_TEXTURE_2D, 0);

  return kErrNone;
}

long GlRenderDevice::BindRectTexture(Texture *pTexture) {
  if (nullptr == pTexture) return kErrInvalidParam;

  GLhandleARB handle = pTexture->GetHandle();
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, handle);

  return kErrNone;
}

long GlRenderDevice::UnbindRectTexture() {
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

  return kErrNone;
}
}  // namespace detail
}  // namespace scenic