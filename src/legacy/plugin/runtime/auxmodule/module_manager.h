// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _SMT_AMODULE_MGR_H
#define _SMT_AMODULE_MGR_H

#include <map>
#include <mutex>

#include "legacy/core/macros/macros.h"
#include "legacy/plugin/runtime/auxmodule/module.h"
#include "legacy/plugin/runtime/auxmodule/plugin_export.h"

namespace plugin {
typedef map<string, SmtAuxModule> SmtAuxModuleMap;

class PLUGIN_EXPORT SmtAModuleManager {
 public:
  virtual ~SmtAModuleManager(void);

 public:
  static SmtAModuleManager* get_singleton_ptr(void);
  static void destroy_instance(void);

 public:
  long notify(SmtAuxModule* pAModule, long lMsg, SmtListenerMsg& param);

  long register_a_module(SmtAuxModule* pAModule);
  long remove_a_module(SmtAuxModule* pAModule);
  long remove_all_a_module(void);

  void set_active_a_module(SmtAuxModule* pAModule) {
    m_pActiveAModules = pAModule;
  }
  SmtAuxModule* get_active_a_module(void) { return m_pActiveAModules; }
  const SmtAuxModule* get_active_a_module(void) const {
    return m_pActiveAModules;
  }

  int get_a_module_count(void) const { return m_vAModulePtrs.size(); }
  SmtAuxModule* get_a_module(int index);
  const SmtAuxModule* get_a_module(int index) const;

  long register_a_module_msg(SmtAuxModule* pAModule);
  long unregister_a_module_msg(SmtAuxModule* pAModule);

 protected:
#ifdef SMT_THREAD_SAFE
  std::mutex m_cslock;
#endif
  vSmtAModulePtrs m_vAModulePtrs;
  SmtAuxModule* m_pActiveAModules;
  mapMsgToPtr m_mapMsgToAModules;

 private:
  SmtAModuleManager(void);
  static SmtAModuleManager* m_pSingleton;
};
}  // namespace plugin

#endif  // _SMT_AMODULE_MGR_H
