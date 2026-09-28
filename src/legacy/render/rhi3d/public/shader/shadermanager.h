// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_SHADERMANAGER_H
#define _RD3D_SHADERMANAGER_H

#include <map>

#include "base/core/log.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/shader/shader.h"

using namespace base;
using namespace render;

namespace render {
typedef vector<SmtShader*> vShaderPtrs;
typedef map<string, SmtShader*> mapNameToShaderPtrs;
typedef pair<string, SmtShader*> pairNameToShaderPtr;

class LEGACY_RENDER_EXPORT SmtShaderManager {
 public:
  SmtShaderManager(void);
  virtual ~SmtShaderManager(void);

 public:
  long AddShader(SmtShader* pShader);
  SmtShader* GetShader(const char* szName);
  void DestroyShader(const char* szName);
  void DestroyAllShader(void);

  void GetAllShaderName(vector<string>& vStrAllShaderName);

 private:
  mapNameToShaderPtrs m_mapNameToShaderPtrs;
};

inline SmtShaderManager::SmtShaderManager(void) { DestroyAllShader(); }

inline SmtShaderManager::~SmtShaderManager(void) { DestroyAllShader(); }

inline long SmtShaderManager::AddShader(SmtShader* pShader) {
  SmtShader* pShaderTmp = NULL;
  pShaderTmp = GetShader(pShader->GetShaderName());
  if (NULL == pShaderTmp)
    m_mapNameToShaderPtrs.insert(
        pairNameToShaderPtr(pShader->GetShaderName(), pShader));
  else {
    LOGGING(LOG_INFO, "AddShader () already exist");

    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

inline SmtShader* SmtShaderManager::GetShader(const char* szName) {
  SmtShader* pShader = NULL;
  mapNameToShaderPtrs::iterator mapIter;
  mapIter = m_mapNameToShaderPtrs.find(szName);

  if (mapIter != m_mapNameToShaderPtrs.end()) {
    pShader = (mapIter->second);
  }

  return pShader;
}

inline void SmtShaderManager::DestroyShader(const char* szName) {
  mapNameToShaderPtrs::iterator iter = m_mapNameToShaderPtrs.find(szName);

  if (iter != m_mapNameToShaderPtrs.end()) {
    SMT_SAFE_DELETE(iter->second);
    m_mapNameToShaderPtrs.erase(iter);
  }
}

inline void SmtShaderManager::DestroyAllShader(void) {
  mapNameToShaderPtrs::iterator i = m_mapNameToShaderPtrs.begin();

  while (i != m_mapNameToShaderPtrs.end()) {
    SMT_SAFE_DELETE(i->second);
    i++;
  }

  m_mapNameToShaderPtrs.clear();
}

inline void SmtShaderManager::GetAllShaderName(
    vector<string>& vStrAllShaderName) {
  vStrAllShaderName.clear();

  mapNameToShaderPtrs::iterator iter = m_mapNameToShaderPtrs.begin();

  while (iter != m_mapNameToShaderPtrs.end()) {
    vStrAllShaderName.push_back(iter->first);
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

#endif  //_RD3D_SHADERSMANAGER_H