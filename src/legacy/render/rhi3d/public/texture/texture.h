// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_TEXTURE_H
#define _RD3D_TEXTURE_H

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_defs.h"

namespace render {
/**
Supported texture formats.
*/
enum TextureFormat {
  RGB8,
  RGBA8,
  RGB_DXT1,
  RGBA_DXT1,
  RGBA_DXT3,
  RGBA_DXT5,
  LUMINANCE8,
  INTENSITY8,
  RGB16F,
  RGBA16F,
  ALPHA16F,
  INTENSITY16F,
  LUMINANCE16F,
  LUMINANCE_ALPHA16F,
  RGB32F,
  RGBA32F,
  ALPHA32F,
  INTENSITY32F,
  LUMINANCE32F,
  LUMINANCE_ALPHA32F
};

enum TextureEnvironment { REPLACE = 0, MODULATE, DECAL, BLEND, ADD };

enum TextureFilter {
  NEAREST = 0,
  LINEAR,
  NEAREST_MIPMAP_NEAREST,  // For minFilter only
  LINEAR_MIPMAP_NEAREST,   // For minFilter only
  NEAREST_MIPMAP_LINEAR,   // For minFilter only
  LINEAR_MIPMAP_LINEAR     // For minFilter only
};

/**
Texture loading mode. Defines compression type
during texture loading.
*/
enum TextureLoadMode { TLM_UNCOMPRESSED = 0, TLM_DXT1, TLM_DXT3, TLM_DXT5 };

/**
Defines behavior of texture mapping when texture coords are out of range.
It is set per texture coordinate and may be different for different coordinates.
*/
enum TextureWrapMode { CLAMP = 0, REPEAT, CLAMP_TO_EDGE };

/**
Memory pool for texture storing.
*/
enum Pool { P_MANAGED, P_DEFAULT, P_SYSMEMORY };

enum Usage { U_UNDEFINED };

struct TextureDesc {
  TextureFormat format;
  int width;
  int height;
  Pool pool;
  Usage usage;
};

struct TextureSampler {
  TextureFilter minFilter;
  TextureFilter magFilter;
  float anisotropy;
  TextureWrapMode sTexture;
  TextureWrapMode tTexture;
  TextureWrapMode rTexture;

  /**
  Default constructor. Sets both filters to bilinear.
  */
  TextureSampler() {
    /* Binilear filtering */
    minFilter = LINEAR;
    magFilter = LINEAR;

    /* Anisotropy is off */
    anisotropy = 0.0;

    /* Texture wrap modes */
    sTexture = CLAMP;
    tTexture = CLAMP;
    rTexture = CLAMP;
  }

  TextureSampler(TextureFilter minFilter, TextureFilter magFilter) {
    /* Set filtering */
    this->minFilter = minFilter;
    this->magFilter = magFilter;

    /* Anisotropy is off */
    anisotropy = 0.0;

    /* Texture wrap modes */
    sTexture = CLAMP;
    tTexture = CLAMP;
    rTexture = CLAMP;
  }
};

struct TextureEnvMode {
  TextureEnvironment envMode;

  TextureEnvMode() { envMode = REPLACE; }

  TextureEnvMode(TextureEnvironment theEnvMode) { envMode = theEnvMode; }
};

class Smt3DRenderDevice;
typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

class LEGACY_RENDER_EXPORT SmtTexture {
 public:
  SmtTexture(LP3DRENDERDEVICE p3DRenderDevice, uint handle, string strName);
  virtual ~SmtTexture();

 public:
  inline uint GetHandle() { return m_unHandle; }
  const char *GetTextureName(void) { return m_strName.c_str(); }

 public:
  TextureSampler GetSampler() const;
  void SetSampler(TextureSampler sampler);

  TextureEnvMode GetEnvMode() const;
  void SetEnvMode(TextureEnvMode envMode);

  TextureDesc GetDesc() const;

 public:
  // 1.
  long Load(string fileName, bool bDynamic = false, bool bUseMips = true);

  // 2.
  long Create(ulong ulWidth, ulong ulHeight, TextureFormat format,
              bool bDynamic = false, bool bUseMips = true);

  long Lock(void);
  long Unlock(void);
  inline bool IsLocked(void) const { return m_bLocked; }

  inline bool IsUseMipmap(void) const { return m_bUseMips; }

 public:
  long Use();
  long Unuse();

 public:
  int GetComponents(TextureFormat format);
  bool IsSizePowerOfTwo(void);

  void *GetData();
  long SetData(void *pData, ulong ulSize);

  void SetPixel4f(float a, float r, float g, float b);
  void SetPixel3f(float r, float g, float b);
  void SetPixel4uc(unsigned char a, unsigned char r, unsigned char g,
                   unsigned char b);
  void SetPixel3uc(unsigned char r, unsigned char g, unsigned char b);

 protected:
  TextureSampler m_texSampler;
  TextureEnvMode m_texEnv;
  TextureDesc m_texDesc;

  bool m_bDynamic;
  bool m_bUseMips;

  ulong m_ulPixelStride;
  bool m_bLocked;

  void *m_pBuffer;
  unsigned char *m_pCurrentPixel;

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  uint m_unHandle;
  string m_strName;
};

inline void SmtTexture::SetPixel4f(float a, float r, float g, float b) {
  if (m_texDesc.format == RGBA8) {
    m_pCurrentPixel[0] = (unsigned char)(255.0f * r);
    m_pCurrentPixel[1] = (unsigned char)(255.0f * g);
    m_pCurrentPixel[2] = (unsigned char)(255.0f * b);
    m_pCurrentPixel[3] = (unsigned char)(255.0f * a);
    m_pCurrentPixel += 4;
  }
}

inline void SmtTexture::SetPixel3f(float r, float g, float b) {
  if (m_texDesc.format == RGB8) {
    m_pCurrentPixel[0] = (unsigned char)(255.0f * r);
    m_pCurrentPixel[1] = (unsigned char)(255.0f * g);
    m_pCurrentPixel[2] = (unsigned char)(255.0f * b);
    m_pCurrentPixel += 3;
  }
}

inline void SmtTexture::SetPixel4uc(unsigned char a, unsigned char r,
                                    unsigned char g, unsigned char b) {
  if (m_texDesc.format == RGBA8) {
    m_pCurrentPixel[0] = r;
    m_pCurrentPixel[1] = g;
    m_pCurrentPixel[2] = b;
    m_pCurrentPixel[3] = a;
    m_pCurrentPixel += 4;
  }
}

inline void SmtTexture::SetPixel3uc(unsigned char r, unsigned char g,
                                    unsigned char b) {
  if (m_texDesc.format == RGB8) {
    m_pCurrentPixel[0] = r;
    m_pCurrentPixel[1] = g;
    m_pCurrentPixel[2] = b;
    m_pCurrentPixel += 3;
  }
}
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD3D_TEXTURE_H

// Bodies call Smt3DRenderDevice. This header is included before that type is
// complete, so the bodies are emitted only from the re-include at the bottom
// of render_device.h.
#if defined(SMT_3DRENDERDEVICE_COMPLETE) && !defined(_RD3D_TEXTURE_METHODS)
#define _RD3D_TEXTURE_METHODS

#include <cstdio>

#include "base/core/log.h"
#include "legacy/core/util/image.h"
#include "ximage.h"

namespace render {

inline SmtTexture::SmtTexture(LP3DRENDERDEVICE p3DRenderDevice, uint handle,
                              string strName)
    : m_unHandle(handle),
      m_p3DRenderDevice(p3DRenderDevice),
      m_strName(strName),
      m_bDynamic(false),
      m_bUseMips(false),
      m_ulPixelStride(0),
      m_bLocked(false),
      m_pBuffer(nullptr),
      m_pCurrentPixel(nullptr) {
  ;
}

inline SmtTexture::~SmtTexture() { ; }

inline long SmtTexture::Use(void) {
  SmtGPUStateManager *stateManager = m_p3DRenderDevice->GetStateManager();

  if (SMT_ERR_NONE == m_p3DRenderDevice->BindTexture(this) &&
      SMT_ERR_NONE == stateManager->SetSampler(m_texSampler) &&
      SMT_ERR_NONE == stateManager->SetTextureEnvironment(m_texEnv)) {
    return SMT_ERR_NONE;
  }

  return SMT_ERR_FAILURE;
}

inline long SmtTexture::Unuse() {
  if (SMT_ERR_NONE == m_p3DRenderDevice->UnbindTexture()) {
    return SMT_ERR_NONE;
  }

  return SMT_ERR_FAILURE;
}

inline TextureSampler SmtTexture::GetSampler() const { return m_texSampler; }

inline void SmtTexture::SetSampler(TextureSampler sampler) {
  m_texSampler = sampler;
}

inline TextureEnvMode SmtTexture::GetEnvMode() const { return m_texEnv; }

inline void SmtTexture::SetEnvMode(TextureEnvMode envMode) {
  m_texEnv = envMode;
}

inline TextureDesc SmtTexture::GetDesc() const { return m_texDesc; }

inline int SmtTexture::GetComponents(TextureFormat format) {
  switch (format) {
    case ALPHA16F:
    case ALPHA32F:
    case INTENSITY8:
    case INTENSITY16F:
    case INTENSITY32F:
    case LUMINANCE8:
    case LUMINANCE16F:
    case LUMINANCE32F:
      return 1;

    case LUMINANCE_ALPHA16F:
    case LUMINANCE_ALPHA32F:
      return 2;

    case RGB8:
    case RGB16F:
    case RGB32F:
    case RGB_DXT1:
      return 3;

    case RGBA8:
    case RGBA_DXT1:
    case RGBA_DXT3:
    case RGBA_DXT5:
    case RGBA16F:
    case RGBA32F:
      return 4;

    default:
      return 0;
  }
}

inline bool SmtTexture::IsSizePowerOfTwo() {
  return (((m_texDesc.width & (m_texDesc.width - 1)) == 0) &&
          ((m_texDesc.height & (m_texDesc.height - 1)) == 0));
}

inline long SmtTexture::Create(ulong ulWidth, ulong ulHeight,
                               TextureFormat format, bool bDynamic,
                               bool bUseMips) {
  m_texDesc.width = ulWidth;
  m_texDesc.height = ulHeight;
  m_texDesc.format = format;
  m_bUseMips = bUseMips;

  if (format == RGB8)
    m_ulPixelStride = 3;
  else
    m_ulPixelStride = 4;

  m_bDynamic = bDynamic;
  m_bLocked = false;

  m_pBuffer = (void *)(new unsigned char[m_ulPixelStride * m_texDesc.width *
                                         m_texDesc.height]);
  ZeroMemory(m_pBuffer, m_ulPixelStride * m_texDesc.width * m_texDesc.height);

  m_pCurrentPixel = (unsigned char *)nullptr;

  return SMT_ERR_NONE;
}

inline long SmtTexture::SetData(void *pData, ulong ulSize) {
  if (!IsLocked()) return SMT_ERR_FAILURE;

  if (m_pBuffer == nullptr || pData == nullptr || ulSize < 1 ||
      ulSize != m_ulPixelStride * m_texDesc.width * m_texDesc.height)
    return SMT_ERR_FAILURE;

  memcpy(m_pBuffer, pData, ulSize);

  return SMT_ERR_NONE;
}

inline void *SmtTexture::GetData() { return m_pBuffer; }

inline long SmtTexture::Lock() {
  m_pCurrentPixel = (unsigned char *)m_pBuffer;
  m_bLocked = true;

  return SMT_ERR_NONE;
}

inline long SmtTexture::Unlock() {
  m_bLocked = false;
  m_pCurrentPixel = (unsigned char *)nullptr;

  if (m_pBuffer != nullptr) {
    m_p3DRenderDevice->BuildTexture(this);
    //...

    SMT_SAFE_DELETE_A(m_pBuffer);
    m_pCurrentPixel = (unsigned char *)m_pBuffer;
  }

  return SMT_ERR_NONE;
}

inline long SmtTexture::Load(string fileName, bool bDynamic, bool bUseMips) {
  int nImageTyle = get_image_type_by_file_ext(fileName.c_str());
  TextureFormat texFmt;
  // CxImage's TCHAR filename ctor is wchar_t when CxImage is built UNICODE.
  FILE *fp = fopen(fileName.c_str(), "rb");
  if (fp == nullptr) return SMT_ERR_FAILURE;
  CxImage img(fp, nImageTyle);
  fclose(fp);

  if (img.IsValid()) {
    if (img.GetBpp() == 24)
      texFmt = RGB8;
    else
      texFmt = RGBA8;

    if (SMT_ERR_NONE == Create(img.GetWidth(), img.GetHeight(), texFmt,
                               bDynamic, bUseMips) &&
        SMT_ERR_NONE == Lock() &&
        SMT_ERR_NONE == SetData(img.GetDIB(), img.GetHeight() * img.GetWidth() *
                                                  (img.GetBpp() / 8)) &&
        SMT_ERR_NONE == Unlock()) {
      return SMT_ERR_NONE;
    } else {
      LOGGING(LOG_INFO, "AddTexture() %s fail", fileName.c_str());

      return SMT_ERR_FAILURE;
    }
  }

  return SMT_ERR_FAILURE;
}

}  // namespace render

#endif  // _RD3D_TEXTURE_METHODS