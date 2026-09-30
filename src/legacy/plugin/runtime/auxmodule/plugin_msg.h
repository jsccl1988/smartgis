// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _AM_MSG_H
#define _AM_MSG_H

#include "legacy/core/msg/msg_def.h"
#include "legacy/plugin/runtime/auxmodule/module_manager.h"
#include "legacy/plugin/runtime/auxmodule/plugin_export.h"
using namespace plugin;


#define SMT_AM_MSG_BROADCAST ((SmtAuxModule *)0xFFFF)
#define SMT_AM_MSG_INVALID ((SmtAuxModule *)0x0000)

#define SMT_POST_AM_MSG(pAModule, lMsg, param)                               \
  {                                                                          \
    SmtAModuleManager *pAModuleMgr = SmtAModuleManager::get_singleton_ptr(); \
    pAModuleMgr->notify(pAModule, lMsg, param);                              \
  }
long PLUGIN_EXPORT SmtPostAMMsg(SmtAuxModule *pAMoudule, long lMsg,
                                SmtListenerMsg &param);

#endif  // _AM_MSG_H
