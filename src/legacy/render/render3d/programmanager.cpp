#include "legacy/render/render3d/programmanager.h"

#include "base/core/log.h"
#include "legacy/render/render3d/3drenderdevice.h"

using namespace base;

namespace render {
//////////////////////////////////////////////////////////////////////////
SmtProgramManager::SmtProgramManager(void) { DestroyAllProgram(); }

SmtProgramManager::~SmtProgramManager(void) { DestroyAllProgram(); }

long SmtProgramManager::AddProgram(SmtProgram *pProgram) {
  SmtProgram *pProgTmp = NULL;
  pProgTmp = GetProgram(pProgram->GetProgramName());
  if (NULL == pProgTmp)
    m_mapNameToProgramPtrs.insert(
        pairNameToProgramPtr(pProgram->GetProgramName(), pProgram));
  else {
    LOGGING(LOG_INFO, "AddProgram () already exist");

    return SMT_ERR_FAILURE;
  }

  return SMT_ERR_NONE;
}

SmtProgram *SmtProgramManager::GetProgram(const char *szName) {
  SmtProgram *pProgram = NULL;
  mapNameToProgramPtrs::iterator mapIter;
  mapIter = m_mapNameToProgramPtrs.find(szName);

  if (mapIter != m_mapNameToProgramPtrs.end()) {
    pProgram = (mapIter->second);
  }

  return pProgram;
}

void SmtProgramManager::DestroyProgram(const char *szName) {
  mapNameToProgramPtrs::iterator iter = m_mapNameToProgramPtrs.find(szName);

  if (iter != m_mapNameToProgramPtrs.end()) {
    SMT_SAFE_DELETE(iter->second);
    m_mapNameToProgramPtrs.erase(iter);
  }
}

void SmtProgramManager::DestroyAllProgram(void) {
  mapNameToProgramPtrs::iterator i = m_mapNameToProgramPtrs.begin();

  while (i != m_mapNameToProgramPtrs.end()) {
    SMT_SAFE_DELETE(i->second);
    i++;
  }

  m_mapNameToProgramPtrs.clear();
}

void SmtProgramManager::GetAllProgramName(vector<string> &vStrAllProgramName) {
  vStrAllProgramName.clear();

  mapNameToProgramPtrs::iterator iter = m_mapNameToProgramPtrs.begin();

  while (iter != m_mapNameToProgramPtrs.end()) {
    vStrAllProgramName.push_back(iter->first);
    iter++;
  }
}
}  // namespace render