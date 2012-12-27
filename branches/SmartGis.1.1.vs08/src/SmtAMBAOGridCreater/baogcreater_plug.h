#ifndef _BAOGRIDCREATER_PLUG_H
#define _BAOGRIDCREATER_PLUG_H
#include "am_amodule.h"
#include "baog_baorthgrid.h"
#include "vw_2deditxview.h"

using namespace Smt_AM;
using namespace Smt_BAOrthGrid;
using namespace Smt_XView;

class SmtBAOGridCreaterPlugin:public SmtAuxModule
{
public:
	SmtBAOGridCreaterPlugin(void);
	virtual ~SmtBAOGridCreaterPlugin(void);

public:
	int							Init(void);
	int							Destroy(void);

public:
	int							Notify(long lMsg,SmtListenerMsg &param);

protected:
	//输入边界控制点
	void						OnInputBnd0(void);
	void						OnInputBnd2(void);

	void						LoadFromFile(void);

protected:
	int							Init2DStuff(void);

	int							CreateIAGetLineTool(void);
	static	int					GetIAToolResult(long nMsg,SmtListenerMsg &param);

public:
	int							OnEndInputBnd(SmtLineString *pLineString);

public:
	SmtBaseTool					*m_pActiveTool;

	vdbfPoints					m_ctrlBnd0;
	vdbfPoints					m_ctrlBnd2;
	int							m_bndIndex;

protected:
	LPRENDERDEVICE           m_pRenderDevice;
	Smt2DEditXView				*m_p2DEditView;
};

#endif //_BAOGRIDCREATER_PLUG_H