#ifndef _AM_MAPPRJ_H
#define _AM_MAPPRJ_H
#include "legacy/plugin/runtime/auxmodule/module.h"

using namespace plugin;

class MapPrjPlugin : public SmtAuxModule {
 public:
  MapPrjPlugin(void);
  virtual ~MapPrjPlugin(void);

 public:
  int Init(void);
  int Destroy(void);

 public:
  int notify(long lMsg, SmtListenerMsg &param);
};

#endif  //_AM_MAPPRJ_H