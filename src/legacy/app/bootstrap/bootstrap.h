// Copyright (c) 2010 CCL. All rights reserved.
#ifndef SMTAPP_H
#define SMTAPP_H

#include "base/core/log.h"
#include "legacy/app/app_export.h"
#include "legacy/core/macros/macros.h"
#include "legacy/core/types/env.h"
#include "legacy/core/msg/msg_def.h"

using namespace base;

namespace app {
class APP_CORE_EXPORT SmtApp {
 public:
  SmtApp(void);
  virtual ~SmtApp(void);

 public:
  bool Init();
  bool DelayInit();
  bool Destory();

 protected:
  // Exe CWinAppEx overrides to restore AFX module state around plugin unload.
  virtual void restore_mfc_module_state();

  bool InitLogMgr(void);
  bool InitStyleMgr(void);
  bool InitCatalogSource(void);
  bool InitSmtMap(void);
  bool InitSmtListenerMgr(void);
  bool InitSmtAuxModules(void);

 private:
  bool m_bInit;
};
}  // namespace app

#if !defined(APP_CORE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "app_core_d.lib")
#else
#pragma comment(lib, "app_core.lib")
#endif
#endif

#endif  // SMTAPP_H