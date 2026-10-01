// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_PROGRAMMANAGER_H
#define _RD3D_PROGRAMMANAGER_H

#include <map>

#include "base/core/log.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/shader/program.h"

using namespace base;
using namespace render;

namespace render {
typedef vector<SmtProgram*> vProgramPtrs;
typedef map<string, SmtProgram*> mapNameToProgramPtrs;
typedef pair<string, SmtProgram*> pairNameToProgramPtr;

class LEGACY_RENDER_EXPORT SmtProgramManager {
 public:
  SmtProgramManager(void);
  virtual ~SmtProgramManager(void);

 public:
  long AddProgram(SmtProgram* pProgram);
  SmtProgram* GetProgram(const char* szName);
  void DestroyProgram(const char* szName);
  void DestroyAllProgram(void);

  void GetAllProgramName(vector<string>& vStrAllProgramName);

 private:
  mapNameToProgramPtrs m_mapNameToProgramPtrs;
};

inline SmtProgramManager::SmtProgramManager(void) { DestroyAllProgram(); }

inline SmtProgramManager::~SmtProgramManager(void) { DestroyAllProgram(); }

inline long SmtProgramManager::AddProgram(SmtProgram* pProgram) {
  SmtProgram* pProgTmp = nullptr;
  pProgTmp = GetProgram(pProgram->GetProgramName());
  if (nullptr == pProgTmp)
    m_mapNameToProgramPtrs.insert(
        pairNameToProgramPtr(pProgram->GetProgramName(), pProgram));
  else {
    LOGGING(LOG_INFO, "AddProgram () already exist");

    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

inline SmtProgram* SmtProgramManager::GetProgram(const char* szName) {
  SmtProgram* pProgram = nullptr;
  mapNameToProgramPtrs::iterator mapIter;
  mapIter = m_mapNameToProgramPtrs.find(szName);

  if (mapIter != m_mapNameToProgramPtrs.end()) {
    pProgram = (mapIter->second);
  }

  return pProgram;
}

inline void SmtProgramManager::DestroyProgram(const char* szName) {
  mapNameToProgramPtrs::iterator iter = m_mapNameToProgramPtrs.find(szName);

  if (iter != m_mapNameToProgramPtrs.end()) {
    SMT_SAFE_DELETE(iter->second);
    m_mapNameToProgramPtrs.erase(iter);
  }
}

inline void SmtProgramManager::DestroyAllProgram(void) {
  mapNameToProgramPtrs::iterator i = m_mapNameToProgramPtrs.begin();

  while (i != m_mapNameToProgramPtrs.end()) {
    SMT_SAFE_DELETE(i->second);
    i++;
  }

  m_mapNameToProgramPtrs.clear();
}

inline void SmtProgramManager::GetAllProgramName(
    vector<string>& vStrAllProgramName) {
  vStrAllProgramName.clear();

  mapNameToProgramPtrs::iterator iter = m_mapNameToProgramPtrs.begin();

  while (iter != m_mapNameToProgramPtrs.end()) {
    vStrAllProgramName.push_back(iter->first);
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

#endif  //_RD3D_PROGRAMMANAGER_H