#include "stdafx.h"
#include "plugin/legacy/proj/map_project.h"
#include "plugin/legacy/proj/mapprj_plug.h"
#include "plugin/legacy/proj/dlg_map_prj.h"
#include "plugin/host/legacy_cmd.h"

#include <cstring>

const string						CST_STR_MAPPRJ_AM_NAME	= "��ͼͶӰ";
SmtMapPrjPlugin						*g_pMapPrj = NULL;

#define  AM_MSG_CMD_MAPPRJ_BEGIN	(SMT_MSG_USER_BEGIN+150)
#define  MAPPRJ_DO_PRJ				(AM_MSG_CMD_MAPPRJ_BEGIN+1)
#define  AM_MSG_CMD_PCREGI_END		(AM_MSG_CMD_MAPPRJ_BEGIN + 50)

static_assert(MAPPRJ_DO_PRJ == plugin::kAmMsgProjDoPrj);

extern "C"
{
	int __declspec(dllexport) GetPluginVersion(void)
	{
		return 1;
	}

	void __declspec(dllexport) StartPlugin(void)
	{
		g_pMapPrj = new SmtMapPrjPlugin();
		if (g_pMapPrj)
		{
			g_pMapPrj->Init();
		}
	}

	void __declspec(dllexport) StopPlugin(void)
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
	set_name(CST_STR_MAPPRJ_AM_NAME.c_str());
}
	
SmtMapPrjPlugin::~SmtMapPrjPlugin(void)
{

}

int SmtMapPrjPlugin::Init(void)
{
	SmtAuxModule::Init();

	append_func_items("ͶӰ����",MAPPRJ_DO_PRJ,FIM_2DMFMENU|FIM_AUXMODULEBOX);
	RegisterMsg();

	return SMT_ERR_NONE;
}

int SmtMapPrjPlugin::Destroy(void)
{
	return SmtAuxModule::Destroy();
}

int SmtMapPrjPlugin::notify(long lMsg,SmtListenerMsg &param)
{
	(void)param;
	const char* id = plugin::command_id_from_am_msg(lMsg);
	long cmd = lMsg;
	if (id && std::strcmp(id, "proj.do_prj") == 0)
		cmd = MAPPRJ_DO_PRJ;
	switch (cmd)
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