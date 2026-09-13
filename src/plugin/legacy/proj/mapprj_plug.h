#ifndef _AM_MAPPRJ_H
#define _AM_MAPPRJ_H
#include "plugin/legacy/module.h"

using namespace plugin;

class SmtMapPrjPlugin:public SmtAuxModule
{
public:
	SmtMapPrjPlugin(void);
	virtual ~SmtMapPrjPlugin(void);

public:
	int              Init(void);
	int              Destroy(void);

public:
	int				 notify(long lMsg,SmtListenerMsg &param);
};

#endif //_AM_MAPPRJ_H