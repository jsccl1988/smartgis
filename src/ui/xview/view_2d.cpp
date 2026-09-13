// Smt2DXView.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "ui/xview/view_core.h"
#include "ui/xview/view_2d.h"

#include "base/core/core.h"
#include "base/core/logmanager.h"
#include "base/style/stylemanager.h"
#include "sys/sysmanager.h"
namespace sdb {
class SmtFeature;
}
#include <vector>

#include "base/core/api.h"
#include "base/core/listenermanager.h"
#include "content/public/view_host.h"
#include "plugin/module_manager.h"
#include "plugin/plugin_msg.h"
#include "sdb/feature/feature.h"
#include "sdb/map/map.h"
#include "tool/group/defs.h"
#include "tool/group/flashtool.h"
#include "tool/t_iatoolmanager.h"
#include "tool/workspace.h"
#include "ui/xcatalog/mapmgr.h"

using namespace base;
using namespace geo;
using namespace sdb;
using namespace render;
using namespace tool;
using namespace sys;
using namespace plugin;

namespace {

void paint_aux_overlay(LPRENDERDEVICE device, const tool::AuxOverlay *overlay) {
  if (!device || !overlay || overlay->points.size() < 2) {
    return;
  }
  SmtSysManager *sys = SmtSysManager::get_singleton_ptr();
  SmtStyleManager *styles = SmtStyleManager::get_singleton_ptr();
  if (!sys || !styles) {
    return;
  }
  SmtStyleConfig cfg = sys->get_sys_style_config();
  SmtStyle *style = styles->get_style(cfg.szAuxStyle);
  if (SMT_ERR_NONE !=
      device->BeginRender(MRD_BL_QUICK, false, style, R2_COPYPEN)) {
    return;
  }
  if (overlay->kind == tool::AuxOverlay::Kind::kRect) {
    fRect frt;
    frt.merge(overlay->points[0].x_px, overlay->points[0].y_px);
    frt.merge(overlay->points[1].x_px, overlay->points[1].y_px);
    device->DrawRect(frt, true);
  } else if (overlay->kind == tool::AuxOverlay::Kind::kPolyline) {
    std::vector<fPoint> pts;
    pts.reserve(overlay->points.size());
    for (const tool::AuxPoint &p : overlay->points) {
      pts.push_back(
          fPoint(static_cast<float>(p.x_px), static_cast<float>(p.y_px)));
    }
    device->DrawLine(pts.data(), static_cast<int>(pts.size()), true);
  }
  device->EndRender(MRD_BL_QUICK);
}

}  // namespace

// Smt2DXView

namespace ui {
IMPLEMENT_DYNCREATE(Smt2DXView, SmtXView)

static void Notify2DXViewOperMap(void *p2DXView, SmtMap *pMap) {
  if (p2DXView != NULL) static_cast<Smt2DXView *>(p2DXView)->SetOperMap(pMap);
}

Smt2DXView::Smt2DXView() {
  m_pRenderer = NULL;
  m_pRenderDevice = NULL;

  m_pViewCtrlTool = NULL;
  m_pSelectTool = NULL;
  m_pFlashTool = NULL;

  m_pSmtOperMap = NULL;
  ui::SmtMapMgr::get_singleton_ptr()->Set2DXViewNotify(&Notify2DXViewOperMap);
}

Smt2DXView::~Smt2DXView() {}

LPRENDERDEVICE Smt2DXView::GetRenderDevice(void) { return m_pRenderDevice; }

SmtMap *Smt2DXView::GetOperMap(void) { return m_pSmtOperMap; }

BEGIN_MESSAGE_MAP(Smt2DXView, SmtXView)
ON_WM_CREATE()
ON_WM_DESTROY()
ON_WM_SIZE()
ON_WM_MOUSEMOVE()
ON_WM_TIMER()
ON_WM_KEYDOWN()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONDOWN()
ON_WM_MOUSEWHEEL()
ON_WM_LBUTTONDBLCLK()
ON_WM_RBUTTONDBLCLK()
ON_WM_RBUTTONUP()
ON_WM_CONTEXTMENU()
ON_WM_SETCURSOR()
ON_WM_ERASEBKGND()

END_MESSAGE_MAP()

// Smt2DXView ��ͼ

void Smt2DXView::OnDraw(CDC *pDC) {
  CDocument *pDoc = GetDocument();
  // TODO: �ڴ����ӻ��ƴ���
  /*if (!m_bActive)
  return;*/

  if (m_pRenderDevice) m_pRenderDevice->RenderMap();

  if (m_pFlashTool) m_pFlashTool->AuxDraw();

  if (view_host() && view_host()->workspace()) {
    view_host()->workspace()->aux_draw();
    paint_aux_overlay(m_pRenderDevice,
                      view_host()->workspace()->live_preview());
  }
}

// Smt2DXView ���

#ifdef _DEBUG
void Smt2DXView::AssertValid() const { SmtXView::AssertValid(); }

#ifndef _WIN32_WCE
void Smt2DXView::Dump(CDumpContext &dc) const { SmtXView::Dump(dc); }
#endif
#endif  //_DEBUG

// Smt2DXView ��Ϣ��������

int Smt2DXView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (SmtXView::OnCreate(lpCreateStruct) == -1) return -1;

  // TODO:  �ڴ�������ר�õĴ�������
  SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();

  SmtSysPra sysPra = pSysMgr->get_sys_pra();

  m_uiRefreshTimer = SetTimer(69, sysPra.l2DViewRefreshTime, 0);
  m_uiNotifyTimer = SetTimer(70, sysPra.l2DViewNotifyTime, 0);

  return 0;
}

void Smt2DXView::OnDestroy() {
  SmtXView::OnDestroy();

  // TODO: �ڴ˴�������Ϣ�����������
  KillTimer(69);
  KillTimer(70);
}

void Smt2DXView::OnSize(UINT nType, int cx, int cy) {
  SmtXView::OnSize(nType, cx, cy);

  // TODO: �ڴ˴�������Ϣ�����������
  if (m_pRenderDevice && cx > 0 && cy > 0) {
    SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtSysPra sysPra = pSysMgr->get_sys_pra();
    lRect lrt;
    Smt2DRenderPra rdPra;

    lrt.lb.x = 0;
    lrt.rt.y = 0;
    lrt.rt.x = cx;
    lrt.lb.y = cy;

    rdPra.bShowMBR = sysPra.bShowMBR;
    rdPra.bShowPoint = sysPra.bShowPoint;
    rdPra.lPointRaduis = sysPra.lPointRaduis;

    m_pRenderDevice->Resize(0, 0, cx, cy);
    m_pRenderDevice->SetRenderPra(rdPra);
    m_pRenderDevice->RefreshDirectly(m_pSmtOperMap, lrt);
  }
}

void Smt2DXView::OnMouseMove(UINT nFlags, CPoint point) {
  SmtXView::OnMouseMove(nFlags, point);
}

void Smt2DXView::OnTimer(UINT_PTR nIDEvent) {
  // TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ
  if (!m_bActive) return;

  switch (nIDEvent) {
    case 69: {
      SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
      SmtBaseTool *pTmpTool =
          dynamic_cast<SmtBaseTool *>(pIAToolMgr->GetActiveIATool());
      if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
        pTmpTool->Timer();
      }

      if (m_pFlashTool) m_pFlashTool->Timer();
    } break;
    case 70: {
      if (m_pRenderDevice && m_bActive) {
        SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
        SmtSysPra sysPra = pSysMgr->get_sys_pra();
        Smt2DRenderPra rdPra;

        rdPra.bShowMBR = sysPra.bShowMBR;
        rdPra.bShowPoint = sysPra.bShowPoint;
        rdPra.lPointRaduis = sysPra.lPointRaduis;

        m_pRenderDevice->SetRenderPra(rdPra);
        m_pRenderDevice->Timer();

        if (m_bActive) PostMessage(WM_PAINT);
      }
    } break;
    default:
      break;
  }

  SmtXView::OnTimer(nIDEvent);
}

void Smt2DXView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
  // TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ
  SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  SmtBaseTool *pTmpTool =
      dynamic_cast<SmtBaseTool *>(pIAToolMgr->GetActiveIATool());
  if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
    pTmpTool->KeyDown(nChar, nRepCnt, nFlags);
  }

  SmtXView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void Smt2DXView::OnLButtonDown(UINT nFlags, CPoint point) {
  SmtXView::OnLButtonDown(nFlags, point);
}

void Smt2DXView::OnLButtonUp(UINT nFlags, CPoint point) {
  SmtXView::OnLButtonUp(nFlags, point);
}

void Smt2DXView::OnRButtonDown(UINT nFlags, CPoint point) {
  SmtXView::OnRButtonDown(nFlags, point);
}

BOOL Smt2DXView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
  return SmtXView::OnMouseWheel(nFlags, zDelta, pt);
}

void Smt2DXView::OnLButtonDblClk(UINT nFlags, CPoint point) {
  SmtXView::OnLButtonDblClk(nFlags, point);
}

void Smt2DXView::OnRButtonDblClk(UINT nFlags, CPoint point) {
  SmtXView::OnRButtonDblClk(nFlags, point);
}

void Smt2DXView::OnRButtonUp(UINT nFlags, CPoint point) {
  SmtXView::OnRButtonUp(nFlags, point);
}

BOOL Smt2DXView::OnEraseBkgnd(CDC *pDC) {
  // TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ
  return FALSE;
}

void Smt2DXView::OnContextMenu(CWnd *pWnd, CPoint point) {
  // TODO: �ڴ˴�������Ϣ�����������
  SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  SmtBaseTool *pTmpTool =
      dynamic_cast<SmtBaseTool *>(pIAToolMgr->GetActiveIATool());
  if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
    if (pTmpTool && !pTmpTool->IsEnableContexMenu()) return;
  }

  CMenu contexMenu;
  contexMenu.Attach(m_hContexMenu);
  contexMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                            point.x, point.y, this);
  contexMenu.Detach();
}

BOOL Smt2DXView::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message) {
  SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  SmtBaseTool *pTmpTool =
      dynamic_cast<SmtBaseTool *>(pIAToolMgr->GetActiveIATool());
  if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
    return pTmpTool->SetCursor();
  }

  return SmtXView::OnSetCursor(pWnd, nHitTest, message);
}

//////////////////////////////////////////////////////////////////////////
bool Smt2DXView::InitCreate(void) { return SmtXView::InitCreate(); }

bool Smt2DXView::EndDestory(void) {
  SmtGroupToolFactory::DestoryGroupTool(m_pViewCtrlTool);
  SmtGroupToolFactory::DestoryGroupTool(m_pSelectTool);
  SmtGroupToolFactory::DestoryGroupTool(m_pFlashTool);

  SMT_SAFE_DELETE(m_pRenderer);

  return SmtXView::EndDestory();
}

bool Smt2DXView::CreateContexMenu() {
  SmtXView::CreateContexMenu();

  SmtLogManager *pLogMgr = SmtLogManager::get_singleton_ptr();
  SmtLog *pLog = pLogMgr->get_default_log();

  // view ctrl menu
  append_listener_menu(m_hContexMenu, m_pViewCtrlTool, FIM_2DVIEW, false);

  // flash menu
  append_listener_menu(m_hContexMenu, m_pFlashTool, FIM_2DVIEW);

  // select menu
  append_listener_menu(m_hContexMenu, m_pSelectTool, FIM_2DVIEW);

  //////////////////////////////////////////////////////////////////////////
  // am menu
  ::AppendMenu(m_hContexMenu, MF_SEPARATOR, NULL, NULL);
  SmtAModuleManager *pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
  if (pAModuleMgr) {
    for (int i = 0; i < pAModuleMgr->get_a_module_count(); i++) {
      SmtAuxModule *pAModule = pAModuleMgr->get_a_module(i);
      HMENU hMenu = create_listener_menu(pAModule, FIM_2DVIEW);
      if (GetMenuItemCount(hMenu) > 0)
        InsertMenu(m_hContexMenu, i + 3, MF_POPUP, (UINT)hMenu,
                   pAModule->get_name());
    }
  }

  pLog->log_message("Init 2DView ContexMenu OK!");

  return true;
}

bool Smt2DXView::CreateMainMenu() {
  SmtXView::CreateMainMenu();

  SmtLogManager *pLogMgr = SmtLogManager::get_singleton_ptr();
  SmtLog *pLog = pLogMgr->get_default_log();

  // view ctrl menu
  HMENU hMenu = create_listener_menu(m_pViewCtrlTool, FIM_2DMFMENU);
  if (GetMenuItemCount(hMenu) > 0)
    AppendMenu(m_hMainMenu, MF_POPUP, (UINT)hMenu, m_pViewCtrlTool->get_name());

  // flash menu
  hMenu = create_listener_menu(m_pFlashTool, FIM_2DMFMENU);
  if (GetMenuItemCount(hMenu) > 0)
    AppendMenu(m_hMainMenu, MF_POPUP, (UINT)hMenu, m_pFlashTool->get_name());

  // select menu
  hMenu = create_listener_menu(m_pSelectTool, FIM_2DMFMENU);
  if (GetMenuItemCount(hMenu) > 0)
    AppendMenu(m_hMainMenu, MF_POPUP, (UINT)hMenu, m_pSelectTool->get_name());

  //////////////////////////////////////////////////////////////////////////
  // am menu
  ::AppendMenu(m_hMainMenu, MF_SEPARATOR, NULL, NULL);
  SmtAModuleManager *pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
  if (pAModuleMgr) {
    for (int i = 0; i < pAModuleMgr->get_a_module_count(); i++) {
      SmtAuxModule *pAModule = pAModuleMgr->get_a_module(i);
      HMENU hMenu = create_listener_menu(pAModule, FIM_2DMFMENU);
      if (GetMenuItemCount(hMenu) > 0)
        AppendMenu(m_hMainMenu, MF_POPUP, (UINT)hMenu, pAModule->get_name());
    }
  }

  pLog->log_message("Init 2DView MainMenu OK!");

  return true;
}

//////////////////////////////////////////////////////////////////////////
bool Smt2DXView::CreateRender(void) {
  SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtLogManager *pLogMgr = SmtLogManager::get_singleton_ptr();

  SmtLog *pLog = pLogMgr->get_default_log();
  SmtSysPra sysPra = pSysMgr->get_sys_pra();

  string strMapRenderDevice = sysPra.str2DRenderDeviceName;
  m_pRenderer = new SmtRenderer(AfxGetInstanceHandle());
  if (m_pRenderer->CreateDevice(strMapRenderDevice.c_str()) == SMT_ERR_FAILURE)
    return false;

  m_pRenderDevice = m_pRenderer->GetDevice();
  if (m_pRenderDevice == NULL) return false;

  if (m_pRenderDevice->Init(m_hWnd, strMapRenderDevice.c_str()) ==
      SMT_ERR_FAILURE) {
    pLog->log_message("Init %s failure!", strMapRenderDevice.c_str());
    return false;
  }

  m_pRenderDevice->SetMapMode(MM_TEXT);

  pLog->log_message("Init %s ok!", strMapRenderDevice.c_str());

  return true;
}

bool Smt2DXView::CreateTools(void) {
  SmtLogManager *pLogMgr = SmtLogManager::get_singleton_ptr();
  SmtLog *pLog = pLogMgr->get_default_log();

  SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();

  SmtGroupToolFactory::CreateGroupTool(m_pViewCtrlTool,
                                       GroupToolType::GTT_ViewControl);
  SmtGroupToolFactory::CreateGroupTool(m_pSelectTool,
                                       GroupToolType::GTT_Select);
  SmtGroupToolFactory::CreateGroupTool(m_pFlashTool, GroupToolType::GTT_Flash);

  if (NULL == m_pViewCtrlTool || NULL == m_pSelectTool ||
      NULL == m_pFlashTool) {
    pLog->log_message("Init GroupTools failure!");
    return false;
  }

  SmtStyleConfig styleConfig = pSysMgr->get_sys_style_config();

  m_pViewCtrlTool->SetToolStyleName(styleConfig.szAuxStyle);
  if (m_pViewCtrlTool->Init(m_pRenderDevice, m_pSmtOperMap, m_hWnd) !=
      SMT_ERR_NONE)
    return false;

  m_pSelectTool->SetToolStyleName(styleConfig.szAuxStyle);
  if (m_pSelectTool->Init(m_pRenderDevice, m_pSmtOperMap, m_hWnd) !=
      SMT_ERR_NONE)
    return false;

  m_pFlashTool->SetToolStyleName(styleConfig.szAuxStyle);
  if (m_pFlashTool->Init(m_pRenderDevice, m_pSmtOperMap, m_hWnd) !=
      SMT_ERR_NONE)
    return false;

  m_pViewCtrlTool->SetActive();

  if (SmtFlashTool *flash = dynamic_cast<SmtFlashTool *>(m_pFlashTool)) {
    flash->bind_workspace(view_host() ? view_host()->workspace() : NULL);
  }

  pLog->log_message("Init GroupTools ok!");

  return true;
}

void Smt2DXView::SetOperMap(SmtMap *pSmtMap) {
  m_pSmtOperMap = pSmtMap;

  if (m_pViewCtrlTool) m_pViewCtrlTool->SetOperMap(m_pSmtOperMap);

  if (m_pSelectTool) m_pSelectTool->SetOperMap(m_pSmtOperMap);

  if (m_pFlashTool) m_pFlashTool->SetOperMap(m_pSmtOperMap);
}

BOOL Smt2DXView::OnCommand(WPARAM wParam, LPARAM lParam) {
  // TODO: �ڴ�����ר�ô����/����û���
  unsigned int unMsg = (wParam);

  if (unMsg >= SMT_MSG_CMD_BEGIN && unMsg <= SMT_MSG_CMD_END) {
    dispatch_menu_command(unMsg);
  } else if (unMsg >= SMT_MSG_USER_BEGIN && unMsg <= SMT_MSG_USER_END) {
    dispatch_menu_command(unMsg);
  }

  return SmtXView::OnCommand(wParam, lParam);
}

void Smt2DXView::apply_workspace_draft(const tool::Draft &draft) {
  if (draft.kind == tool::DraftKind::kWheel) {
    if (m_pViewCtrlTool) {
      m_pViewCtrlTool->apply_draft(draft);
    }
    return;
  }
  SmtIAToolManager *mgr = SmtIAToolManager::get_singleton_ptr();
  SmtBaseTool *tool =
      mgr ? dynamic_cast<SmtBaseTool *>(mgr->GetActiveIATool()) : NULL;
  if (tool && tool->GetOwnerWnd() == m_hWnd) {
    tool->apply_draft(draft);
  }
}
}  // namespace ui