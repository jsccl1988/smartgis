// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _AM_AMODULE_H
#define _AM_AMODULE_H

#include "legacy/core/listener/listener_manager.h"
#include "legacy/plugin/runtime/auxmodule/plugin_export.h"

using namespace base;

namespace plugin {
class PLUGIN_EXPORT SmtAuxModule : public SmtListener {
 public:
  SmtAuxModule(void);
  virtual ~SmtAuxModule(void);

 public:
  virtual int Register(void);
  virtual int RegisterMsg(void);

  virtual int UnRegister(void);
  virtual int UnRegisterMsg(void);

  virtual int SetActive();

 public:
  virtual int Init(void);
  virtual int Destroy(void);
};

typedef vector<SmtAuxModule*> vSmtAModulePtrs;
}  // namespace plugin

#endif  // _AM_AMODULE_H
