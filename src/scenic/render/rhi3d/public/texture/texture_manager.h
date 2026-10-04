// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_TEXTUREMANAGER_H
#define _RD3D_TEXTUREMANAGER_H

#include <map>

#include "base/core/log.h"
#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/texture/texture.h"

using namespace base;
using namespace scenic::detail;

namespace scenic {
namespace detail {
typedef vector<Texture*> vTexturePtrs;
typedef map<string, Texture*> mapNameToTexturePtrs;
typedef pair<string, Texture*> pairNameToTexturePtr;

class LEGACY_RENDER_EXPORT TextureManager {
 public:
  TextureManager(void);
  virtual ~TextureManager(void);

 public:
  //	static Texture*				CreateTexture(LP3DRENDERDEVICE
  // pRenderDevice, FileInfo &info, const string strName);

 public:
  long AddTexture(Texture* pTexture);
  Texture* GetTexture(const char* szName);
  long DestroyTexture(const char* szName);
  long DestroyAllTexture(void);

  void GetAllTextureName(vector<string>& vStrAllTextureName);

 private:
  mapNameToTexturePtrs m_mapNameToTexturePtrs;
};

inline TextureManager::TextureManager(void) { DestroyAllTexture(); }

inline TextureManager::~TextureManager(void) { DestroyAllTexture(); }

inline long TextureManager::AddTexture(Texture* pTexture) {
  if (nullptr == pTexture) return SMT_ERR_FAILURE;
  Texture* pTexTmp = nullptr;
  pTexTmp = GetTexture(pTexture->GetTextureName());
  if (nullptr == pTexTmp)
    m_mapNameToTexturePtrs.insert(
        pairNameToTexturePtr(pTexture->GetTextureName(), pTexture));
  else {
    LOGGING(LOG_INFO, "AddTexture () already exist");
    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

inline Texture* TextureManager::GetTexture(const char* szName) {
  Texture* pTexture = nullptr;
  mapNameToTexturePtrs::iterator mapIter;
  mapIter = m_mapNameToTexturePtrs.find(szName);

  if (mapIter != m_mapNameToTexturePtrs.end()) {
    pTexture = (mapIter->second);
  }

  return pTexture;
}

inline long TextureManager::DestroyTexture(const char* szName) {
  mapNameToTexturePtrs::iterator iter = m_mapNameToTexturePtrs.find(szName);

  if (iter != m_mapNameToTexturePtrs.end()) {
    SMT_SAFE_DELETE(iter->second);
    m_mapNameToTexturePtrs.erase(iter);
  }

  return SMT_ERR_NONE;
}

inline long TextureManager::DestroyAllTexture(void) {
  mapNameToTexturePtrs::iterator i = m_mapNameToTexturePtrs.begin();

  while (i != m_mapNameToTexturePtrs.end()) {
    SMT_SAFE_DELETE(i->second);
    i++;
  }

  m_mapNameToTexturePtrs.clear();

  return SMT_ERR_NONE;
}

inline void TextureManager::GetAllTextureName(
    vector<string>& vStrAllTextureName) {
  vStrAllTextureName.clear();

  mapNameToTexturePtrs::iterator iter = m_mapNameToTexturePtrs.begin();

  while (iter != m_mapNameToTexturePtrs.end()) {
    vStrAllTextureName.push_back(iter->first);
    iter++;
  }
}
}  // namespace detail
}  // namespace scenic

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD3D_TEXTUREMANAGER_H