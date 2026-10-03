#include "stdafx.h"
#include "legacy/plugin/product/dem/shell/demcreater_plug.h"

#include <cstring>

#include "legacy/plugin/runtime/bridge/cmd.h"
#include "legacy/plugin/product/dem/shell/dem_creater.h"
#include "legacy/plugin/product/dem/views/dlg_about.h"
#include "legacy/plugin/product/dem/views/dlg_grid_loader.h"
#include "legacy/plugin/product/dem/views/dlg_tin_loader.h"
#include "legacy/plugin/runtime/auxmodule/plugin_msg.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/abi/t_iatoolmanager.h"
#include "legacy/tool/abi/t_msg.h"
const string CST_STR_DEMCREATER_AM_NAME = "DEM";
SmtDemCreaterPlugin *g_pDemCreater = NULL;

#define AM_MSG_CMD_DEMCREATER_BEGIN (SMT_MSG_USER_BEGIN + 100)
#define TIN_LOAD_ASSII_FILE (AM_MSG_CMD_DEMCREATER_BEGIN + 1)
#define GRID_LOAD_HEIGHT_MAP (AM_MSG_CMD_DEMCREATER_BEGIN + 2)
#define TIN_LOAD_ABOUT (AM_MSG_CMD_DEMCREATER_BEGIN + 3)
#define AM_MSG_CMD_DEMCREATER_END (AM_MSG_CMD_DEMCREATER_BEGIN + 50)

static_assert(TIN_LOAD_ASSII_FILE == plugin::kAmMsgDemLoadTin);
static_assert(GRID_LOAD_HEIGHT_MAP == plugin::kAmMsgDemLoadGrid);
static_assert(TIN_LOAD_ABOUT == plugin::kAmMsgDemAbout);

extern "C" {
int __declspec(dllexport) GetPluginVersion(void) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());
  return 1;
}

void __declspec(dllexport) StartPlugin(void) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());
  g_pDemCreater = new SmtDemCreaterPlugin();
  if (g_pDemCreater) {
    g_pDemCreater->Init();
  }
}

void __declspec(dllexport) StopPlugin(void) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());
  if (g_pDemCreater) {
    g_pDemCreater->Destroy();
  }
  SMT_SAFE_DELETE(g_pDemCreater);
}
}

SmtDemCreaterPlugin::SmtDemCreaterPlugin(void) {
  set_name(CST_STR_DEMCREATER_AM_NAME.c_str());
}

SmtDemCreaterPlugin::~SmtDemCreaterPlugin(void) {}

int SmtDemCreaterPlugin::Init(void) {
  SmtAuxModule::Init();

  append_func_items("离散点生成DEM", TIN_LOAD_ASSII_FILE,
                    FIM_3DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("高度图生成DEM", GRID_LOAD_HEIGHT_MAP,
                    FIM_3DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("关于", TIN_LOAD_ABOUT, FIM_3DMFMENU | FIM_AUXMODULEBOX);

  RegisterMsg();

  return SMT_ERR_NONE;
}

int SmtDemCreaterPlugin::Destroy(void) { return SmtAuxModule::Destroy(); }

int SmtDemCreaterPlugin::notify(long lMsg, SmtListenerMsg &param) {
  (void)param;
  const char *id = plugin::command_id_from_am_msg(lMsg);
  long cmd = lMsg;
  if (id && std::strcmp(id, "world3d.load_tin") == 0)
    cmd = TIN_LOAD_ASSII_FILE;
  else if (id && std::strcmp(id, "world3d.load_grid") == 0)
    cmd = GRID_LOAD_HEIGHT_MAP;
  else if (id && std::strcmp(id, "world3d.about") == 0)
    cmd = TIN_LOAD_ABOUT;
  switch (cmd) {
    case TIN_LOAD_ASSII_FILE: {
      AFX_MANAGE_STATE(AfxGetStaticModuleState());
      CDlgTinLoader dlg;
      if (dlg.DoModal() == IDOK) {
        ;
      }
    } break;
    case GRID_LOAD_HEIGHT_MAP: {
      AFX_MANAGE_STATE(AfxGetStaticModuleState());
      CDlgGridLoader dlg;
      if (dlg.DoModal() == IDOK) {
        ;
      }
    } break;
    case TIN_LOAD_ABOUT: {
      AFX_MANAGE_STATE(AfxGetStaticModuleState());
      CDlgAbout dlg;
      if (dlg.DoModal() == IDOK) {
        ;
      }
    } break;
  }
  return SMT_ERR_NONE;
}