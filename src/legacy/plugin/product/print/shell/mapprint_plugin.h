#ifndef _MAPPRINT_PLUG_H
#define _MAPPRINT_PLUG_H
#include "legacy/plugin/runtime/auxmodule/module.h"

using namespace plugin;

class MapPrintPlugin : public SmtAuxModule {
 public:
  MapPrintPlugin(void);
  virtual ~MapPrintPlugin(void);

 public:
  int Init(void);
  int Destroy(void);

 public:
  int notify(long lMsg, SmtListenerMsg &param);
};

#endif  //_MAPPRINT_PLUG_H