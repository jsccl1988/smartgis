#ifndef _MAPSERVICEMGR_PLUG_H
#define _MAPSERVICEMGR_PLUG_H
#include "am_amodule.h"

using namespace Smt_AM;

class SmtMapServiceMgrPlugin:public SmtAuxModule
{
public:
	SmtMapServiceMgrPlugin(void);
	virtual ~SmtMapServiceMgrPlugin(void);

public:
	int              Init(void);
	int              Destroy(void);

public:
	int				 Notify(long lMsg,SmtListenerMsg &param);
};

#endif //_MAPSERVICEMGR_PLUG_H