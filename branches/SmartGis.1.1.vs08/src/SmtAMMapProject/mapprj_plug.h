#ifndef _AM_MAPPRJ_H
#define _AM_MAPPRJ_H
#include "am_amodule.h"

using namespace Smt_AM;

class SmtMapPrjPlugin:public SmtAuxModule
{
public:
	SmtMapPrjPlugin(void);
	virtual ~SmtMapPrjPlugin(void);

public:
	int              Init(void);
	int              Destroy(void);

public:
	int				 Notify(long lMsg,SmtListenerMsg &param);
};

#endif //_AM_MAPPRJ_H