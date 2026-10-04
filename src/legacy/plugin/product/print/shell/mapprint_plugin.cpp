#include "stdafx.h"
#include "legacy/plugin/product/print/shell/mapprint_plugin.h"

#include <cstring>

#include "legacy/plugin/runtime/bridge/cmd.h"
#include "legacy/plugin/runtime/auxmodule/plugin_msg.h"
#include "legacy/plugin/product/print/views/dlg_2d_xview.h"
#include "legacy/plugin/product/print/shell/map_print.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/abi/t_msg.h"
#include "legacy/ui/dialogs/dialogs_api.h"
#include "legacy/ui/catalog/map/mapmgr.h"
using namespace gis;
using namespace sys;

const string CST_STR_MAPPRINT_AM_NAME = "Print";
MapPrintPlugin *g_pAMMapPrint = NULL;

#define AM_MSG_CMD_MAPPRINT_BEGIN (SMT_MSG_USER_BEGIN + 200)
#define CMD_DLG_2DXVIEW (AM_MSG_CMD_MAPPRINT_BEGIN + 1)

#define AM_MSG_CMD_MAPPRINT_END (AM_MSG_CMD_MAPPRINT_BEGIN + 50)

static_assert(CMD_DLG_2DXVIEW == plugin::kAmMsgPrintPreview);

extern "C" {
int __declspec(dllexport) GetPluginVersion(void) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());
  return 1;
}

void __declspec(dllexport) StartPlugin(void) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());
  g_pAMMapPrint = new MapPrintPlugin();
  if (g_pAMMapPrint) {
    g_pAMMapPrint->Init();
  }
}

void __declspec(dllexport) StopPlugin(void) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());
  if (g_pAMMapPrint) {
    g_pAMMapPrint->Destroy();
  }

  SMT_SAFE_DELETE(g_pAMMapPrint);
}
}

MapPrintPlugin::MapPrintPlugin(void) {
  set_name(CST_STR_MAPPRINT_AM_NAME.c_str());
}

MapPrintPlugin::~MapPrintPlugin(void) {}

int MapPrintPlugin::Init(void) {
  SmtAuxModule::Init();

  append_func_items("打印", CMD_DLG_2DXVIEW, FIM_2DMFMENU | FIM_AUXMODULEBOX);

  RegisterMsg();

  return SMT_ERR_NONE;
}

int MapPrintPlugin::Destroy(void) { return SmtAuxModule::Destroy(); }

int MapPrintPlugin::notify(long lMsg, SmtListenerMsg &param) {
  (void)param;
  const char *id = plugin::command_id_from_am_msg(lMsg);
  long cmd = lMsg;
  if (id && std::strcmp(id, "print.preview") == 0) cmd = CMD_DLG_2DXVIEW;
  switch (cmd) {
    case CMD_DLG_2DXVIEW: {
      AFX_MANAGE_STATE(AfxGetStaticModuleState());
      CDlg2DXView dlg;
      if (dlg.DoModal() == IDOK) {
        ;
      }
    } break;
  }

  return SMT_ERR_NONE;
}