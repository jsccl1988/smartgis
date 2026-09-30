// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_TEXTUREMANAGER_H
#define _RD3D_TEXTUREMANAGER_H

#include <map>

#include "base/core/log.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/texture/texture.h"

using namespace base;
using namespace render;

namespace render {
typedef vector<SmtTexture*> vTexturePtrs;
typedef map<string, SmtTexture*> mapNameToTexturePtrs;
typedef pair<string, SmtTexture*> pairNameToTexturePtr;

class LEGACY_RENDER_EXPORT SmtTextureManager {
 public:
  SmtTextureManager(void);
  virtual ~SmtTextureManager(void);

 public:
  //	static SmtTexture*				CreateTexture(LP3DRENDERDEVICE
  // pRenderDevice,SmtFileInfo &info,const string strName);

 public:
  long AddTexture(SmtTexture* pTexture);
  SmtTexture* GetTexture(const char* szName);
  long DestroyTexture(const char* szName);
  long DestroyAllTexture(void);

  void GetAllTextureName(vector<string>& vStrAllTextureName);

 private:
  mapNameToTexturePtrs m_mapNameToTexturePtrs;
};

inline SmtTextureManager::SmtTextureManager(void) { DestroyAllTexture(); }

inline SmtTextureManager::~SmtTextureManager(void) { DestroyAllTexture(); }

inline long SmtTextureManager::AddTexture(SmtTexture* pTexture) {
  if (NULL == pTexture) return SMT_ERR_FAILURE;
  SmtTexture* pTexTmp = NULL;
  pTexTmp = GetTexture(pTexture->GetTextureName());
  if (NULL == pTexTmp)
    m_mapNameToTexturePtrs.insert(
        pairNameToTexturePtr(pTexture->GetTextureName(), pTexture));
  else {
    LOGGING(LOG_INFO, "AddTexture () already exist");
    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

inline SmtTexture* SmtTextureManager::GetTexture(const char* szName) {
  SmtTexture* pTexture = NULL;
  mapNameToTexturePtrs::iterator mapIter;
  mapIter = m_mapNameToTexturePtrs.find(szName);

  if (mapIter != m_mapNameToTexturePtrs.end()) {
    pTexture = (mapIter->second);
  }

  return pTexture;
}

inline long SmtTextureManager::DestroyTexture(const char* szName) {
  mapNameToTexturePtrs::iterator iter = m_mapNameToTexturePtrs.find(szName);

  if (iter != m_mapNameToTexturePtrs.end()) {
    SMT_SAFE_DELETE(iter->second);
    m_mapNameToTexturePtrs.erase(iter);
  }

  return SMT_ERR_NONE;
}

inline long SmtTextureManager::DestroyAllTexture(void) {
  mapNameToTexturePtrs::iterator i = m_mapNameToTexturePtrs.begin();

  while (i != m_mapNameToTexturePtrs.end()) {
    SMT_SAFE_DELETE(i->second);
    i++;
  }

  m_mapNameToTexturePtrs.clear();

  return SMT_ERR_NONE;
}

inline void SmtTextureManager::GetAllTextureName(
    vector<string>& vStrAllTextureName) {
  vStrAllTextureName.clear();

  mapNameToTexturePtrs::iterator iter = m_mapNameToTexturePtrs.begin();

  while (iter != m_mapNameToTexturePtrs.end()) {
    vStrAllTextureName.push_back(iter->first);
    iter++;
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

#endif  //_RD3D_TEXTUREMANAGER_H