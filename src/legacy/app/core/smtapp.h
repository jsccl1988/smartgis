// Copyright (c) 2010 CCL. All rights reserved.
#ifndef SMTAPP_H
#define SMTAPP_H

#include "base/core/log.h"
#include "legacy/app/app_export.h"
#include "legacy/core/core.h"
#include "legacy/core/env_struct.h"
#include "legacy/core/msg_def.h"

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
  bool InitLogMgr(void);
  bool InitStyleMgr(void);
  bool InitSmtDataSource(void);
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