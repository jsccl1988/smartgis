#ifndef _3DMODELCREATER_PLUGIN_H
#define _3DMODELCREATER_PLUGIN_H
#include "am_amodule.h"

using namespace Smt_AM;

class Smt3DModelCreaterPlugin:public SmtAuxModule
{
public:
	Smt3DModelCreaterPlugin(void);
	virtual ~Smt3DModelCreaterPlugin(void);

public:
	int              Init(void);
	int              Destroy(void);

public:
	int				 Notify(long lMsg,SmtListenerMsg &param);
};

#endif //_3DMODELCREATER_PLUGIN_H