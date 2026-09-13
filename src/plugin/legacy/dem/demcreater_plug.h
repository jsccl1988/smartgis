#ifndef _DEMCREATER_PLUG_H
#define _DEMCREATER_PLUG_H
#include "plugin/legacy/module.h"

using namespace plugin;

class SmtDemCreaterPlugin:public SmtAuxModule
{
public:
	SmtDemCreaterPlugin(void);
	virtual ~SmtDemCreaterPlugin(void);

public:
	int              Init(void);
	int              Destroy(void);

public:
	int				 notify(long lMsg,SmtListenerMsg &param);
};

#endif //_DEMCREATER_PLUG_H