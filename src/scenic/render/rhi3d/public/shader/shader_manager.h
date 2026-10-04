// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_SHADERMANAGER_H
#define _RD3D_SHADERMANAGER_H

#include <map>

#include "base/core/log.h"
#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/shader/shader.h"

using namespace base;
using namespace scenic::detail;

namespace scenic {
namespace detail {
typedef vector<Shader*> vShaderPtrs;
typedef map<string, Shader*> mapNameToShaderPtrs;
typedef pair<string, Shader*> pairNameToShaderPtr;

class SCENIC_IMPL_EXPORT ShaderManager {
 public:
  ShaderManager(void);
  virtual ~ShaderManager(void);

 public:
  long AddShader(Shader* pShader);
  Shader* GetShader(const char* szName);
  void DestroyShader(const char* szName);
  void DestroyAllShader(void);

  void GetAllShaderName(vector<string>& vStrAllShaderName);

 private:
  mapNameToShaderPtrs m_mapNameToShaderPtrs;
};

inline ShaderManager::ShaderManager(void) { DestroyAllShader(); }

inline ShaderManager::~ShaderManager(void) { DestroyAllShader(); }

inline long ShaderManager::AddShader(Shader* pShader) {
  Shader* pShaderTmp = nullptr;
  pShaderTmp = GetShader(pShader->GetShaderName());
  if (nullptr == pShaderTmp)
    m_mapNameToShaderPtrs.insert(
        pairNameToShaderPtr(pShader->GetShaderName(), pShader));
  else {
    LOGGING(LOG_INFO, "AddShader () already exist");

    return kErrFailure;
  }

  return kErrNone;
}

inline Shader* ShaderManager::GetShader(const char* szName) {
  Shader* pShader = nullptr;
  mapNameToShaderPtrs::iterator mapIter;
  mapIter = m_mapNameToShaderPtrs.find(szName);

  if (mapIter != m_mapNameToShaderPtrs.end()) {
    pShader = (mapIter->second);
  }

  return pShader;
}

inline void ShaderManager::DestroyShader(const char* szName) {
  mapNameToShaderPtrs::iterator iter = m_mapNameToShaderPtrs.find(szName);

  if (iter != m_mapNameToShaderPtrs.end()) {
    SAFE_DELETE(iter->second);
    m_mapNameToShaderPtrs.erase(iter);
  }
}

inline void ShaderManager::DestroyAllShader(void) {
  mapNameToShaderPtrs::iterator i = m_mapNameToShaderPtrs.begin();

  while (i != m_mapNameToShaderPtrs.end()) {
    SAFE_DELETE(i->second);
    i++;
  }

  m_mapNameToShaderPtrs.clear();
}

inline void ShaderManager::GetAllShaderName(
    vector<string>& vStrAllShaderName) {
  vStrAllShaderName.clear();

  mapNameToShaderPtrs::iterator iter = m_mapNameToShaderPtrs.begin();

  while (iter != m_mapNameToShaderPtrs.end()) {
    vStrAllShaderName.push_back(iter->first);
    iter++;
  }
}
}  // namespace detail
}  // namespace scenic

#if !defined(SCENIC_IMPL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD3D_SHADERSMANAGER_H