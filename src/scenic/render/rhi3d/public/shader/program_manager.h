// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_PROGRAMMANAGER_H
#define _RD3D_PROGRAMMANAGER_H

#include <map>

#include "base/core/log.h"
#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/shader/program.h"

using namespace base;
using namespace scenic::detail;

namespace scenic {
namespace detail {
typedef vector<Program*> vProgramPtrs;
typedef map<string, Program*> mapNameToProgramPtrs;
typedef pair<string, Program*> pairNameToProgramPtr;

class LEGACY_RENDER_EXPORT ProgramManager {
 public:
  ProgramManager(void);
  virtual ~ProgramManager(void);

 public:
  long AddProgram(Program* pProgram);
  Program* GetProgram(const char* szName);
  void DestroyProgram(const char* szName);
  void DestroyAllProgram(void);

  void GetAllProgramName(vector<string>& vStrAllProgramName);

 private:
  mapNameToProgramPtrs m_mapNameToProgramPtrs;
};

inline ProgramManager::ProgramManager(void) { DestroyAllProgram(); }

inline ProgramManager::~ProgramManager(void) { DestroyAllProgram(); }

inline long ProgramManager::AddProgram(Program* pProgram) {
  Program* pProgTmp = nullptr;
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

inline Program* ProgramManager::GetProgram(const char* szName) {
  Program* pProgram = nullptr;
  mapNameToProgramPtrs::iterator mapIter;
  mapIter = m_mapNameToProgramPtrs.find(szName);

  if (mapIter != m_mapNameToProgramPtrs.end()) {
    pProgram = (mapIter->second);
  }

  return pProgram;
}

inline void ProgramManager::DestroyProgram(const char* szName) {
  mapNameToProgramPtrs::iterator iter = m_mapNameToProgramPtrs.find(szName);

  if (iter != m_mapNameToProgramPtrs.end()) {
    SMT_SAFE_DELETE(iter->second);
    m_mapNameToProgramPtrs.erase(iter);
  }
}

inline void ProgramManager::DestroyAllProgram(void) {
  mapNameToProgramPtrs::iterator i = m_mapNameToProgramPtrs.begin();

  while (i != m_mapNameToProgramPtrs.end()) {
    SMT_SAFE_DELETE(i->second);
    i++;
  }

  m_mapNameToProgramPtrs.clear();
}

inline void ProgramManager::GetAllProgramName(
    vector<string>& vStrAllProgramName) {
  vStrAllProgramName.clear();

  mapNameToProgramPtrs::iterator iter = m_mapNameToProgramPtrs.begin();

  while (iter != m_mapNameToProgramPtrs.end()) {
    vStrAllProgramName.push_back(iter->first);
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

#endif  //_RD3D_PROGRAMMANAGER_H