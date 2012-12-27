#ifndef _MAPPRINT_PLUG_H
#define _MAPPRINT_PLUG_H
#include "am_amodule.h"

using namespace Smt_AM;

class SmtMapPrintPlugin:public SmtAuxModule
{
public:
	SmtMapPrintPlugin(void);
	virtual ~SmtMapPrintPlugin(void);

public:
	int              Init(void);
	int              Destroy(void);

public:
	int				 Notify(long lMsg,SmtListenerMsg &param);
};

#endif //_MAPPRINT_PLUG_H