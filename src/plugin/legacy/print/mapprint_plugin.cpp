#include "stdafx.h"
#include "plugin/legacy/print/map_print.h"
#include "plugin/legacy/print/mapprint_plugin.h"

#include "base/core/api.h"
#include "legacy/ui/gui/gui_api.h"

#include "legacy/ui/xcatalog/mapmgr.h"
#include "sys/sysmanager.h"
#include "legacy/tool/group/defs.h"
#include "plugin/legacy/plugin_msg.h"
#include "legacy/tool/t_msg.h"

#include "plugin/legacy/print/dlg_2d_xview.h"
#include "plugin/host/legacy_cmd.h"

#include <cstring>

using namespace sdb;
using namespace sys;

const string							CST_STR_MAPPRINT_AM_NAME	= "��ͼ��ӡ";
SmtMapPrintPlugin						*g_pAMMapPrint = NULL;

#define  AM_MSG_CMD_MAPPRINT_BEGIN		(SMT_MSG_USER_BEGIN+200)
#define  CMD_DLG_2DXVIEW				(AM_MSG_CMD_MAPPRINT_BEGIN+1)

#define  AM_MSG_CMD_MAPPRINT_END		(AM_MSG_CMD_MAPPRINT_BEGIN+50)

static_assert(CMD_DLG_2DXVIEW == plugin::kAmMsgPrintPreview);

extern "C"
{
	int __declspec(dllexport) GetPluginVersion(void)
	{
		return 1;
	}

	void __declspec(dllexport) StartPlugin(void)
	{
		g_pAMMapPrint = new SmtMapPrintPlugin();
		if (g_pAMMapPrint)
		{
			g_pAMMapPrint->Init();
		}
	}

	void __declspec(dllexport) StopPlugin(void)
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
	set_name(CST_STR_MAPPRINT_AM_NAME.c_str());
}
	
SmtMapPrintPlugin::~SmtMapPrintPlugin(void)
{

}

int SmtMapPrintPlugin::Init(void)
{
	SmtAuxModule::Init();

	append_func_items("��ӡ",CMD_DLG_2DXVIEW,FIM_2DMFMENU|FIM_AUXMODULEBOX);
	
	RegisterMsg();

	return SMT_ERR_NONE;
}

int SmtMapPrintPlugin::Destroy(void)
{
	return SmtAuxModule::Destroy();
}

int SmtMapPrintPlugin::notify(long lMsg,SmtListenerMsg &param)
{
	(void)param;
	const char* id = plugin::command_id_from_am_msg(lMsg);
	long cmd = lMsg;
	if (id && std::strcmp(id, "print.preview") == 0)
		cmd = CMD_DLG_2DXVIEW;
	switch (cmd)
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