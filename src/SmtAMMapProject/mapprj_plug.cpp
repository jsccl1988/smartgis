#include "stdafx.h"
#include "SmtAMMapProject.h"
#include "mapprj_plug.h"
#include "DlgMapPrj.h"

const string						CST_STR_MAPPRJ_AM_NAME	= "地图投影";
SmtMapPrjPlugin						*g_pMapPrj = NULL;

#define  AM_MSG_CMD_MAPPRJ_BEGIN	(SMT_MSG_USER_BEGIN+150)
#define  MAPPRJ_DO_PRJ				(AM_MSG_CMD_MAPPRJ_BEGIN+1)
#define  AM_MSG_CMD_PCREGI_END		(AM_MSG_CMD_MAPPRJ_BEGIN + 50)

extern "C"
{
	int SMT_EXPORT_DLL GetPluginVersion(void)
	{
		return 1;
	}

	void SMT_EXPORT_DLL StartPlugin(void)
	{
		g_pMapPrj = new SmtMapPrjPlugin();
		if (g_pMapPrj)
		{
			g_pMapPrj->Init();
		}
	}

	void SMT_EXPORT_DLL StopPlugin(void)
	{
		if (g_pMapPrj)
		{
			g_pMapPrj->Destroy();
		}
		SMT_SAFE_DELETE(g_pMapPrj);
	}
}


SmtMapPrjPlugin::SmtMapPrjPlugin(void)
{
	SetName(CST_STR_MAPPRJ_AM_NAME.c_str());
}
	
SmtMapPrjPlugin::~SmtMapPrjPlugin(void)
{

}

int SmtMapPrjPlugin::Init(void)
{
	SmtAuxModule::Init();

	AppendFuncItems("投影换算",MAPPRJ_DO_PRJ,FIM_2DMFMENU|FIM_AUXMODULEBOX);
	RegisterMsg();

	return SMT_ERR_NONE;
}

int SmtMapPrjPlugin::Destroy(void)
{
	return SmtAuxModule::Destroy();
}

int SmtMapPrjPlugin::Notify(long lMsg,SmtListenerMsg &param)
{
	switch (lMsg)
	{
	case MAPPRJ_DO_PRJ:
		{
			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			CDlgMapPrj dlg;
			if (dlg.DoModal() == IDOK)
			{
				;
			}
		}
		break;
	}
	return SMT_ERR_NONE;
}