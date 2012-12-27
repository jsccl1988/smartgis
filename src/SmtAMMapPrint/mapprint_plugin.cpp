#include "stdafx.h"
#include "SmtAMMapPrint.h"
#include "mapprint_plugin.h"

#include "smt_api.h"
#include "smt_gui_api.h"

#include "cata_mapmgr.h"
#include "sys_sysmanager.h"
#include "gt_defs.h"
#include "am_msg.h"
#include "t_msg.h"

#include "Dlg2DXView.h"

using namespace Smt_GIS;
using namespace Smt_Sys;

const string							CST_STR_MAPPRINT_AM_NAME	= "µÿÕº¥Ú”°";
SmtMapPrintPlugin						*g_pAMMapPrint = NULL;

#define  AM_MSG_CMD_MAPPRINT_BEGIN		(SMT_MSG_USER_BEGIN+200)
#define  CMD_DLG_2DXVIEW				(AM_MSG_CMD_MAPPRINT_BEGIN+1)

#define  AM_MSG_CMD_MAPPRINT_END		(AM_MSG_CMD_MAPPRINT_BEGIN+50)

extern "C"
{
	int SMT_EXPORT_DLL GetPluginVersion(void)
	{
		return 1;
	}

	void SMT_EXPORT_DLL StartPlugin(void)
	{
		g_pAMMapPrint = new SmtMapPrintPlugin();
		if (g_pAMMapPrint)
		{
			g_pAMMapPrint->Init();
		}
	}

	void SMT_EXPORT_DLL StopPlugin(void)
	{
		if (g_pAMMapPrint)
		{
			g_pAMMapPrint->Destroy();
		}

		SMT_SAFE_DELETE(g_pAMMapPrint);
	}
}

SmtMapPrintPlugin::SmtMapPrintPlugin(void)
{
	SetName(CST_STR_MAPPRINT_AM_NAME.c_str());
}
	
SmtMapPrintPlugin::~SmtMapPrintPlugin(void)
{

}

int SmtMapPrintPlugin::Init(void)
{
	SmtAuxModule::Init();

	AppendFuncItems("¥Ú”°",CMD_DLG_2DXVIEW,FIM_2DMFMENU|FIM_AUXMODULEBOX);
	
	RegisterMsg();

	return SMT_ERR_NONE;
}

int SmtMapPrintPlugin::Destroy(void)
{
	return SmtAuxModule::Destroy();
}

int SmtMapPrintPlugin::Notify(long lMsg,SmtListenerMsg &param)
{
	switch (lMsg)
	{
	case CMD_DLG_2DXVIEW:
		{
			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			CDlg2DXView dlg;
			if (dlg.DoModal() == IDOK)
			{
				;
			}
		}
		break;
	}

	return SMT_ERR_NONE;
}