#include "stdafx.h"
#include "SmtAMDemCreater.h"
#include "demcreater_plug.h"
#include "DlgTinLoader.h"
#include "DlgGridLoader.h"
#include "DlgAbout.h"

#include "am_msg.h"
#include "t_msg.h"
#include "am_msg.h"
#include "t_iatoolmanager.h"
#include "gt_defs.h"

const string						CST_STR_DEMCREATER_AM_NAME	= "DEM生成";
SmtDemCreaterPlugin					*g_pDemCreater = NULL;

#define  AM_MSG_CMD_DEMCREATER_BEGIN	(SMT_MSG_USER_BEGIN+100)
#define  TIN_LOAD_ASSII_FILE			(AM_MSG_CMD_DEMCREATER_BEGIN+1)
#define  GRID_LOAD_HEIGHT_MAP			(AM_MSG_CMD_DEMCREATER_BEGIN+2)
#define  TIN_LOAD_ABOUT					(AM_MSG_CMD_DEMCREATER_BEGIN+3)
#define  AM_MSG_CMD_DEMCREATER_END		(AM_MSG_CMD_DEMCREATER_BEGIN+50)

extern "C"
{
	int SMT_EXPORT_DLL GetPluginVersion(void)
	{
		return 1;
	}

	void SMT_EXPORT_DLL StartPlugin(void)
	{
		g_pDemCreater = new SmtDemCreaterPlugin();
		if (g_pDemCreater)
		{
			g_pDemCreater->Init();
		}
	}

	void SMT_EXPORT_DLL StopPlugin(void)
	{
		if (g_pDemCreater)
		{
			g_pDemCreater->Destroy();
		}
		SMT_SAFE_DELETE(g_pDemCreater);
	}
}

SmtDemCreaterPlugin::SmtDemCreaterPlugin(void)
{
	SetName(CST_STR_DEMCREATER_AM_NAME.c_str());
}
	
SmtDemCreaterPlugin::~SmtDemCreaterPlugin(void)
{
	
}

int SmtDemCreaterPlugin::Init(void)
{
	SmtAuxModule::Init();

	AppendFuncItems("离散点生成DEM",TIN_LOAD_ASSII_FILE,FIM_3DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("高度图生成DEM",GRID_LOAD_HEIGHT_MAP,FIM_3DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("关于",TIN_LOAD_ABOUT,FIM_3DMFMENU|FIM_AUXMODULEBOX);
	
	RegisterMsg();

	return SMT_ERR_NONE;
}

int SmtDemCreaterPlugin::Destroy(void)
{
	return SmtAuxModule::Destroy();
}

int SmtDemCreaterPlugin::Notify(long lMsg,SmtListenerMsg &param)
{
	switch (lMsg)
	{
	case TIN_LOAD_ASSII_FILE:
		{
			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			CDlgTinLoader dlg;
			if (dlg.DoModal() == IDOK)
			{
				;
			}
		}
		break;
	case GRID_LOAD_HEIGHT_MAP:
		{
			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			CDlgGridLoader dlg;
			if (dlg.DoModal() == IDOK)
			{
				;
			}
		}
		break;
	case TIN_LOAD_ABOUT:
		{
			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			CDlgAbout dlg;
			if (dlg.DoModal() == IDOK)
			{
				;
			}
		}
		break;
	}
	return SMT_ERR_NONE;
}