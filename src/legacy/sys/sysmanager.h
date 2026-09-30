// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Legacy product singleton for style / map-doc / project / sys parameters.
// Compiled into base.dll (BASE_EXPORTS / NET_EXPORTS on that DLL); callers are
// leftover MFC/tools only.

#ifndef LEGACY_SYS_SYSMANAGER_H_
#define LEGACY_SYS_SYSMANAGER_H_

#include "base/core/export.h"
#include "base/core/log.h"
#include "legacy/core/macros/macros.h"
#include "legacy/core/types/env.h"
#include "legacy/core/msg/msg_def.h"

using namespace base;

namespace sys {

// Holds leftover app-wide style, map document, project, and sys parameters.
class BASE_EXPORT SmtSysManager {
 private:
  SmtSysManager(void);

 public:
  virtual ~SmtSysManager(void);

 public:
  static SmtSysManager* get_singleton_ptr(void);
  static void destroy_instance(void);

 public:
  inline SmtStyleConfig get_sys_style_config(void) const {
    return m_styleConfig;
  }
  inline void set_sys_style_config(SmtStyleConfig& config) {
    m_styleConfig = config;
  }

  inline SmtMapDocInfo get_sys_map_doc_info(void) const { return m_mapDocInfo; }
  inline void set_sys_map_doc_info(SmtMapDocInfo& mapDocInfo) {
    m_mapDocInfo = mapDocInfo;
  }

  inline SmtPrjInfo get_sys_prj_info(void) const { return m_prjInfo; }
  inline void set_sys_prj_info(SmtPrjInfo& prjInfo) { m_prjInfo = prjInfo; }

  inline SmtSysPra get_sys_pra(void) const { return m_sysPra; }
  inline void set_sys_pra(const SmtSysPra& sysPra) { m_sysPra = sysPra; }

 private:
  SmtStyleConfig m_styleConfig;
  SmtMapDocInfo m_mapDocInfo;
  SmtPrjInfo m_prjInfo;

  SmtSysPra m_sysPra;

 private:
  static SmtSysManager* m_pSingleton;
};

}  // namespace sys

#if !defined(BASE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "base_d.lib")
#else
#pragma comment(lib, "base.lib")
#endif
#endif

#endif  // LEGACY_SYS_SYSMANAGER_H_
