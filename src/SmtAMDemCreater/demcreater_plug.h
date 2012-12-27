#ifndef _DEMCREATER_PLUG_H
#define _DEMCREATER_PLUG_H
#include "am_amodule.h"

using namespace Smt_AM;

class SmtDemCreaterPlugin:public SmtAuxModule
{
public:
	SmtDemCreaterPlugin(void);
	virtual ~SmtDemCreaterPlugin(void);

public:
	int              Init(void);
	int              Destroy(void);

public:
	int				 Notify(long lMsg,SmtListenerMsg &param);
};

#endif //_DEMCREATER_PLUG_H