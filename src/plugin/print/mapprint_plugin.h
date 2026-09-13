#ifndef _MAPPRINT_PLUG_H
#define _MAPPRINT_PLUG_H
#include "plugin/module.h"

using namespace plugin;

class SmtMapPrintPlugin:public SmtAuxModule
{
public:
	SmtMapPrintPlugin(void);
	virtual ~SmtMapPrintPlugin(void);

public:
	int              Init(void);
	int              Destroy(void);

public:
	int				 notify(long lMsg,SmtListenerMsg &param);
};

#endif //_MAPPRINT_PLUG_H