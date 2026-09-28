// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/sys/sysmanager.h"

#include "legacy/core/api.h"
#include "legacy/core/core_exception.h"

namespace sys {

SmtSysManager* SmtSysManager::m_pSingleton = NULL;

SmtSysManager* SmtSysManager::get_singleton_ptr(void) {
  if (m_pSingleton == NULL) {
    m_pSingleton = new SmtSysManager();
  }
  return m_pSingleton;
}

void SmtSysManager::destroy_instance(void) { SMT_SAFE_DELETE(m_pSingleton); }

SmtSysManager::SmtSysManager(void) {}

SmtSysManager::~SmtSysManager(void) {}

}  // namespace sys
