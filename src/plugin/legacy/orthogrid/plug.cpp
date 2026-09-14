#include "stdafx.h"
#include "plugin/legacy/orthogrid/plug.h"

#include <cstring>

#include "base/core/api.h"
#include "base/core/listenermanager.h"
#include "base/style/stylemanager.h"
#include "plugin/host/legacy_cmd.h"
#include "plugin/legacy/orthogrid/creater.h"
#include "plugin/legacy/plugin_msg.h"
#include "sys/sysmanager.h"
#include "legacy_tool/group/defs.h"
#include "legacy_tool/t_iatoolmanager.h"
#include "legacy_tool/t_msg.h"
#include "legacy_ui/gui/gui_api.h"
#include "legacy_ui/xcatalog/mapmgr.h"
#include "legacy_ui/xview/view_2d_edit.h"

using namespace sdb;
using namespace sys;
using namespace base;
using namespace ui;

const string CST_STR_ORTHOGRID_PLUG_NAME = "�߽���Ӧ��������";
OrthogridPlugin *g_pOrthogrid = NULL;

#define AM_MSG_CMD_ORTHOGRID_BEGIN (SMT_MSG_USER_BEGIN + 50)
#define ORTHOGRID_INPUT_BOUDARY0 (AM_MSG_CMD_ORTHOGRID_BEGIN + 1)
#define ORTHOGRID_INPUT_BOUDARY2 (AM_MSG_CMD_ORTHOGRID_BEGIN + 2)
#define ORTHOGRID_SAVE_BOUDARY (AM_MSG_CMD_ORTHOGRID_BEGIN + 3)
#define ORTHOGRID_LOAD_BOUDARY (AM_MSG_CMD_ORTHOGRID_BEGIN + 4)
#define AM_MSG_CMD_ORTHOGRID_END (AM_MSG_CMD_ORTHOGRID_BEGIN + 50)

static_assert(ORTHOGRID_INPUT_BOUDARY0 ==
              plugin::kAmMsgOrthogridInputBoundary0);
static_assert(ORTHOGRID_INPUT_BOUDARY2 ==
              plugin::kAmMsgOrthogridInputBoundary2);
static_assert(ORTHOGRID_SAVE_BOUDARY == plugin::kAmMsgOrthogridSaveBoundary);
static_assert(ORTHOGRID_LOAD_BOUDARY == plugin::kAmMsgOrthogridLoadBoundary);

extern "C" {
int __declspec(dllexport) GetPluginVersion(void) { return 1; }

void __declspec(dllexport) StartPlugin(void) {
  g_pOrthogrid = new OrthogridPlugin();
  if (g_pOrthogrid) {
    g_pOrthogrid->Init();
  }
}

void __declspec(dllexport) StopPlugin(void) {
  if (g_pOrthogrid) {
    g_pOrthogrid->Destroy();
  }

  SMT_SAFE_DELETE(g_pOrthogrid);
}
}

OrthogridPlugin::OrthogridPlugin(void)
    : m_pActiveTool(NULL),
      m_bndIndex(-1),
      m_pRenderDevice(NULL),
      m_p2DEditView(NULL) {
  set_name(CST_STR_ORTHOGRID_PLUG_NAME.c_str());
}

OrthogridPlugin::~OrthogridPlugin(void) { SMT_SAFE_DELETE(m_pActiveTool); }

int OrthogridPlugin::Init(void) {
  SmtAuxModule::Init();

  append_func_items("Input boundary 0", ORTHOGRID_INPUT_BOUDARY0,
                    FIM_2DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("Input boundary 2", ORTHOGRID_INPUT_BOUDARY2,
                    FIM_2DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("Save boundary", ORTHOGRID_SAVE_BOUDARY,
                    FIM_2DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("Load boundary", ORTHOGRID_LOAD_BOUDARY,
                    FIM_2DMFMENU | FIM_AUXMODULEBOX);

  RegisterMsg();

  return SMT_ERR_NONE;
}

int OrthogridPlugin::Destroy(void) { return SmtAuxModule::Destroy(); }

int OrthogridPlugin::notify(long lMsg, SmtListenerMsg &param) {
  (void)param;
  const char *id = plugin::command_id_from_am_msg(lMsg);
  // Product digitize path is leftover GTT_InputLine, including when chrome
  // posts the mapped edit.append.linestring id / GT_MSG_APPEND_LINESTRING.
  if (id && std::strcmp(id, "edit.append.linestring") == 0) {
    if (SMT_ERR_NONE != Init2DStuff()) return SMT_ERR_FAILURE;
    if (m_bndIndex != 0) return OnInputBnd0();
    return OnInputBnd2();
  }

  long cmd = lMsg;
  if (id && (std::strcmp(id, "baogrid.input_boundary_0") == 0 ||
             std::strcmp(id, "orthogrid.input_boundary_0") == 0))
    cmd = ORTHOGRID_INPUT_BOUDARY0;
  else if (id && (std::strcmp(id, "baogrid.input_boundary_2") == 0 ||
                  std::strcmp(id, "orthogrid.input_boundary_2") == 0))
    cmd = ORTHOGRID_INPUT_BOUDARY2;
  else if (id && (std::strcmp(id, "baogrid.save_boundary") == 0 ||
                  std::strcmp(id, "orthogrid.save_boundary") == 0))
    cmd = ORTHOGRID_SAVE_BOUDARY;
  else if (id && (std::strcmp(id, "baogrid.load_boundary") == 0 ||
                  std::strcmp(id, "orthogrid.load_boundary") == 0))
    cmd = ORTHOGRID_LOAD_BOUDARY;

  if (cmd < AM_MSG_CMD_ORTHOGRID_BEGIN || cmd > AM_MSG_CMD_ORTHOGRID_END)
    return SMT_ERR_NONE;

  if (SMT_ERR_NONE != Init2DStuff()) return SMT_ERR_FAILURE;

  switch (cmd) {
    case ORTHOGRID_INPUT_BOUDARY0:
      return OnInputBnd0();
    case ORTHOGRID_INPUT_BOUDARY2:
      return OnInputBnd2();
    case ORTHOGRID_SAVE_BOUDARY:
      break;
    case ORTHOGRID_LOAD_BOUDARY:
      LoadFromFile();
      break;
  }
  return SMT_ERR_NONE;
}

int OrthogridPlugin::OnInputBnd0(void) {
  // Leftover exclusive mouse stays GTT_InputLine (not a plugin InputEvent).
  m_ctrlBnd0.clear();
  m_ctrlBnd2.clear();
  m_bndIndex = 0;

  if (SMT_ERR_NONE == CreateIAGetLineTool()) {
    ushort unType = LT_LineString;
    SmtListenerMsg param0;
    param0.hSrcWnd = m_pActiveTool->GetOwnerWnd();
    param0.wParam = WPARAM(&unType);
    m_pActiveTool->notify(GT_MSG_SET_INPUT_LINE_TYPE, param0);
    m_pActiveTool->SetActive();
  }
  return SMT_ERR_NONE;
}

int OrthogridPlugin::OnInputBnd2(void) {
  if (m_bndIndex != 0) return SMT_ERR_FAILURE;

  m_bndIndex = 2;

  if (SMT_ERR_NONE == CreateIAGetLineTool()) {
    ushort unType = LT_LineString;
    SmtListenerMsg param0;
    param0.hSrcWnd = m_pActiveTool->GetOwnerWnd();
    param0.wParam = WPARAM(&unType);
    m_pActiveTool->notify(GT_MSG_SET_INPUT_LINE_TYPE, param0);
    m_pActiveTool->SetActive();
  }
  return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
int OrthogridPlugin::Init2DStuff(void) {
  SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
  SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();
  SmtVectorLayer *pVLayer = NULL;

  if (NULL != pLayer && pLayer->GetLayerType() == LYR_VECTOR) {
    SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
    if (leftover_layer_feature_type(pVLayer) != SmtFtGrid) {
      ::MessageBox(::GetActiveWindow(), "�뼤��GRIDͼ��!", "��ʾ", MB_OK);
      return SMT_ERR_FAILURE;
    }
  } else
    return SMT_ERR_FAILURE;

  if (NULL != m_p2DEditView && NULL != m_p2DEditView->GetSafeHwnd())
    return SMT_ERR_NONE;

  SmtListenerMsg msgParam;
  msgParam.lParam = (LPARAM)&m_p2DEditView;
  smt_post_listener_msg(SMT_LISTENER_MSG_BROADCAST, SMT_MSG_GET_SYS_2DEDITVIEW,
                        msgParam);

  if (NULL == m_p2DEditView || NULL == m_p2DEditView->GetSafeHwnd())
    return SMT_ERR_FAILURE;

  m_pRenderDevice = m_p2DEditView->GetRenderDevice();

  if (NULL == m_pRenderDevice) return SMT_ERR_FAILURE;

  return SMT_ERR_NONE;
}

int OrthogridPlugin::CreateIAGetLineTool(void) {
  SMT_SAFE_DELETE(m_pActiveTool);

  // ����tool
  SmtGroupToolFactory::CreateGroupTool(m_pActiveTool,
                                       GroupToolType::GTT_InputLine);

  if (NULL == m_pActiveTool) return SMT_ERR_FAILURE;

  SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtStyleConfig styleConfig = pSysMgr->get_sys_style_config();

  m_pActiveTool->SetToolStyleName(styleConfig.szLineStyle);

  if (m_pActiveTool->Init(m_p2DEditView->GetRenderDevice(),
                          m_p2DEditView->GetOperMap(),
                          m_p2DEditView->GetSafeHwnd(), GetIAToolResult,
                          (void *)this) != SMT_ERR_NONE)
    return SMT_ERR_FAILURE;

  return SMT_ERR_NONE;
}

int OrthogridPlugin::GetIAToolResult(long nMsg, SmtListenerMsg &param) {
  OrthogridPlugin *pThis = (OrthogridPlugin *)param.pToFollow;

  ushort uRetType = *(ushort *)param.lParam;

  if (uRetType != GT_MSG_RET_INPUT_LINE) return SMT_ERR_FAILURE;

  pThis->OnEndInputBnd((OGRLineString *)param.wParam);

  return SMT_ERR_NONE;
}

int OrthogridPlugin::OnEndInputBnd(OGRLineString *pLineString) {
  if (m_bndIndex == 0) {
    int nNum = pLineString->getNumPoints();
    m_ctrlBnd0.resize(nNum);
    for (int i = 0; i < nNum; i++) {
      m_ctrlBnd0[i].x = pLineString->getX(i);
      m_ctrlBnd0[i].y = pLineString->getY(i);
    }
  } else if (m_bndIndex == 2) {
    int nNum = pLineString->getNumPoints();
    m_ctrlBnd2.resize(nNum);
    for (int i = 0; i < nNum; i++) {
      m_ctrlBnd2[i].x = pLineString->getX(i);
      m_ctrlBnd2[i].y = pLineString->getY(i);
    }

    m_bndIndex = -1;

    vdbfPoints ctrlBnd1;
    vdbfPoints ctrlBnd3;

    ctrlBnd1.push_back(m_ctrlBnd0[m_ctrlBnd0.size() - 1]);
    ctrlBnd1.push_back(m_ctrlBnd2[0]);

    ctrlBnd3.push_back(m_ctrlBnd2[m_ctrlBnd2.size() - 1]);
    ctrlBnd3.push_back(m_ctrlBnd0[0]);

    SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
    SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();
    SmtVectorLayer *pVLayer = NULL;
    if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType())
      return SMT_ERR_FAILURE;

    pVLayer = (SmtVectorLayer *)pLayer;

    if (NULL != pVLayer && SmtFtGrid == leftover_layer_feature_type(pVLayer)) {
      SmtGrid oSmtGrid;
      Orthogrid orthGrid(33, 17);

      orthGrid.SetMainRegoinBoudary(m_ctrlBnd0, ctrlBnd1, m_ctrlBnd2, ctrlBnd3);

      if (SMT_ERR_NONE == orthGrid.CreateOrthGrid() &&
          SMT_ERR_NONE == orthGrid.CvtToGrid(oSmtGrid)) {
        string strAppTempPath = get_app_temp_path();
        strAppTempPath += "last_bfc_bnd.txt";
        orthGrid.SaveGridBndToFile(strAppTempPath.c_str());

        SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
        SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

        SmtFeature *pSmtFeature = new SmtFeature;

        pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtGrid);
        pSmtFeature->SetStyle(styleSonfig.szPointStyle);
        pSmtFeature->SetGeometry(&oSmtGrid);

        if (pSmtMapMgr->AppendFeature(pSmtFeature, false)) {
          SmtListenerMsg param;
          ::MessageBox(::GetActiveWindow(), "���ɳɹ�!", "��ʾ", MB_OK);
          (void)plugin::command_id_from_am_msg(GT_MSG_VIEW_ZOOMREFRESH);
          post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST, GT_MSG_VIEW_ZOOMREFRESH,
                           param);
        } else
          SMT_SAFE_DELETE(pSmtFeature);
      }
    }
  }

  return SMT_ERR_NONE;
}

void OrthogridPlugin::LoadFromFile(void) {
  SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
  SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

  if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType()) return;

  SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;

  if (pVLayer && leftover_layer_feature_type(pVLayer) == SmtFtGrid) {
    static char BASED_CODE szFilter[] = "Data Files (*.txt)|*.txt";

    CFileDialog dlg(true, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    szFilter, NULL);

    if (dlg.DoModal() == IDCANCEL) return;

    SmtGrid oSmtGrid;
    Orthogrid orthGrid;

    if (SMT_ERR_NONE == orthGrid.LoadGridBndFromFile(dlg.GetPathName()) &&
        SMT_ERR_NONE == orthGrid.CreateOrthGrid() &&
        SMT_ERR_NONE == orthGrid.CvtToGrid(oSmtGrid)) {
      string strAppTempPath = get_app_temp_path();
      strAppTempPath += "last_bfc_bnd.txt";
      orthGrid.SaveGridBndToFile(strAppTempPath.c_str());

      SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
      SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

      SmtFeature *pSmtFeature = new SmtFeature;

      pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtGrid);
      pSmtFeature->SetStyle(styleSonfig.szPointStyle);
      pSmtFeature->SetGeometry(&oSmtGrid);

      if (pSmtMapMgr->AppendFeature(pSmtFeature, false)) {
        SmtListenerMsg param;
        ::MessageBox(::GetActiveWindow(), "���ɳɹ�!", "��ʾ", MB_OK);
        (void)plugin::command_id_from_am_msg(GT_MSG_VIEW_ZOOMREFRESH);
        post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST, GT_MSG_VIEW_ZOOMREFRESH,
                         param);
      } else
        SMT_SAFE_DELETE(pSmtFeature);
    }
  }
}