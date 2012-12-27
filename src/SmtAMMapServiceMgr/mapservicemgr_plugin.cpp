#include "stdafx.h"
#include "SmtAMMapServiceMgr.h"
#include "mapservicemgr_plugin.h"

#include "smt_api.h"
#include "smt_gui_api.h"

#include "cata_mapmgr.h"
#include "sys_sysmanager.h"
#include "gt_defs.h"
#include "am_msg.h"
#include "t_msg.h"

#include "DlgMapServiceMgr.h"

using namespace Smt_GIS;
using namespace Smt_Sys;

const string								CST_STR_MAPSERVICEMGR_AM_NAME	= "地图服务管理";
SmtMapServiceMgrPlugin						*g_pAMMapServiceMgr = NULL;

#define  AM_MSG_CMD_MAPSERVICEMGR_BEGIN		(SMT_MSG_USER_BEGIN+400)
#define  CMD_DLG_MAPSERVICEMGR				(AM_MSG_CMD_MAPSERVICEMGR_BEGIN+1)

#define  AM_MSG_CMD_MAPSERVICEMGR_END		(AM_MSG_CMD_MAPSERVICEMGR_BEGIN+50)

extern "C"
{
	int SMT_EXPORT_DLL GetPluginVersion(void)
	{
		return 1;
	}

	void SMT_EXPORT_DLL StartPlugin(void)
	{
		g_pAMMapServiceMgr = new SmtMapServiceMgrPlugin();
		if (g_pAMMapServiceMgr)
		{
			g_pAMMapServiceMgr->Init();
		}
	}

	void SMT_EXPORT_DLL StopPlugin(void)
	{
		if (g_pAMMapServiceMgr)
		{
			g_pAMMapServiceMgr->Destroy();
		}

		SMT_SAFE_DELETE(g_pAMMapServiceMgr);
	}
}

SmtMapServiceMgrPlugin::SmtMapServiceMgrPlugin(void)
{
	SetName(CST_STR_MAPSERVICEMGR_AM_NAME.c_str());
}
	
SmtMapServiceMgrPlugin::~SmtMapServiceMgrPlugin(void)
{

}

int SmtMapServiceMgrPlugin::Init(void)
{
	SmtAuxModule::Init();

	AppendFuncItems("地图服务管理",CMD_DLG_MAPSERVICEMGR,FIM_2DMFMENU|FIM_AUXMODULEBOX);
	
	RegisterMsg();

	return SMT_ERR_NONE;
}

int SmtMapServiceMgrPlugin::Destroy(void)
{
	return SmtAuxModule::Destroy();
}

int SmtMapServiceMgrPlugin::Notify(long lMsg,SmtListenerMsg &param)
{
	switch (lMsg)
	{
	case CMD_DLG_MAPSERVICEMGR:
		{
			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			CDlgMapServiceMgr dlg;
			if (dlg.DoModal() == IDOK)
			{
				;
			}
		}
		break;
	}

	return SMT_ERR_NONE;
}