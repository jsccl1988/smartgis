// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"

#include "legacy/ui/map/viewport/view_2d.h"

#include <chrono>
#include <cstdio>
#include <cstring>

#include "base/core/log.h"
#include "content/public/view_host.h"
#include "gis/model/feature/feature.h"
#include "gis/model/map/map.h"
#include "legacy/core/listener/listener_manager.h"
#include "legacy/core/macros/macros.h"
#include "legacy/core/util/menu.h"
#include "legacy/gis/present/carto/stylemanager.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/tool/abi/t_iatoolmanager.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/nav/viewctrltool.h"
#include "legacy/tool/select/flashtool.h"
#include "legacy/tool/select/selecttool.h"
#include "legacy/ui/catalog/map/mapmgr.h"
#include "legacy/ui/map/chrome/aux_overlay.h"
#include "legacy/ui/map/chrome/identity_hud.h"
#include "legacy/ui/map/menu/am_menu.h"
#include "legacy/ui/map/tools/bind_workspace.h"
#include "legacy/ui/shell/resource.h"
#include "tool/draft/draft.h"
#include "tool/nav/camera_nav.h"
#include "tool/workspace/workspace.h"

using namespace base;
using namespace geo;
using namespace gis;
using namespace render;
using namespace tool;
using namespace sys;

namespace ui {
IMPLEMENT_DYNCREATE(Smt2DXView, SmtXView)

#define SMT_MSG_FRAME_OPER_MAP (WM_APP + 0x2D01)

static void Notify2DXViewOperMap(void* p2DXView, SmtMap* pMap) {
  if (p2DXView != NULL) {
    static_cast<Smt2DXView*>(p2DXView)->SetOperMap(pMap);
  }
}

Smt2DXView::Smt2DXView() {
  m_pRenderer = NULL;
  m_pRenderDevice = NULL;
  m_pViewCtrlTool = NULL;
  m_pSelectTool = NULL;
  m_pFlashTool = NULL;
  m_pSmtOperMap = NULL;
  m_uiRefreshTimer = 0;
  m_uiNotifyTimer = 0;
  ui::SmtMapMgr::get_singleton_ptr()->Set2DXViewNotify(&Notify2DXViewOperMap);
}

Smt2DXView::~Smt2DXView() {}

LPRENDERDEVICE Smt2DXView::GetRenderDevice(void) { return m_pRenderDevice; }

SmtMap* Smt2DXView::GetOperMap(void) { return m_pSmtOperMap; }

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
ON_MESSAGE(SMT_MSG_FRAME_OPER_MAP, OnFrameOperMap)
ON_MESSAGE(WM_APP + 0x4354, OnDeferredContextMenu)
END_MESSAGE_MAP()

void Smt2DXView::OnDraw(CDC* pDC) {
  CDocument* pDoc = GetDocument();
  (void)pDoc;

  float paint_ms = 0.f;
  if (m_pRenderDevice) {
    HDC paint_dc = pDC ? pDC->GetSafeHdc() : nullptr;
    const auto t0 = std::chrono::steady_clock::now();
    m_pRenderDevice->RenderMapToDC(paint_dc);
    paint_ms = std::chrono::duration<float, std::milli>(
                   std::chrono::steady_clock::now() - t0)
                   .count();
  }

  if (m_pFlashTool) {
    m_pFlashTool->AuxDraw();
  }

  if (view_host() && view_host()->workspace()) {
    view_host()->workspace()->aux_draw();
    detail::paint_aux_overlay(m_pRenderDevice,
                              view_host()->workspace()->live_preview());
  }

  char label[160];
  std::snprintf(label, sizeof(label),
                "legacy-map2d-gdi | Legacy Map2D (GDI+)  paint %.1f ms",
                paint_ms);
  detail::paint_identity_hud(m_hWnd, pDC ? pDC->GetSafeHdc() : nullptr, label);
}

#ifdef _DEBUG
void Smt2DXView::AssertValid() const { SmtXView::AssertValid(); }
#ifndef _WIN32_WCE
void Smt2DXView::Dump(CDumpContext& dc) const { SmtXView::Dump(dc); }
#endif
#endif

int Smt2DXView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (SmtXView::OnCreate(lpCreateStruct) == -1) {
    return -1;
  }
  LOGGING(LOG_INFO, "Smt2DXView::OnCreate after InitCreate (timers deferred)");
  return 0;
}

void Smt2DXView::OnDestroy() {
  KillTimer(69);
  KillTimer(70);
  KillTimer(71);
  MSG msg;
  while (::PeekMessage(&msg, m_hWnd, WM_TIMER, WM_TIMER, PM_REMOVE)) {
  }
  m_pRenderDevice = nullptr;
  SmtXView::OnDestroy();
}

void Smt2DXView::OnSize(UINT nType, int cx, int cy) {
  SmtXView::OnSize(nType, cx, cy);

  if (m_oper_map_frame.framing) {
    return;
  }

  if (m_pRenderDevice && cx > 0 && cy > 0) {
    SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtSysPra sysPra = pSysMgr->get_sys_pra();
    lRect lrt;
    Smt2DRenderOptions rdOptions;

    lrt.lb.x = 0;
    lrt.rt.y = 0;
    lrt.rt.x = cx;
    lrt.lb.y = cy;

    rdOptions.bShowMBR = sysPra.bShowMBR;
    rdOptions.bShowPoint = sysPra.bShowPoint;
    rdOptions.lPointRaduis = sysPra.lPointRaduis;

    m_pRenderDevice->Resize(0, 0, cx, cy);
    m_pRenderDevice->SetRenderOptions(rdOptions);
    if (m_pSmtOperMap && !m_oper_map_frame.framed) {
      request_oper_map_frame();
    } else {
      m_pRenderDevice->RefreshDirectly(m_pSmtOperMap, lrt);
    }
  }
}

void Smt2DXView::OnMouseMove(UINT nFlags, CPoint point) {
  SmtXView::OnMouseMove(nFlags, point);
}

void Smt2DXView::OnTimer(UINT_PTR nIDEvent) {
  if (!m_bActive || !m_pRenderDevice) {
    return;
  }

  switch (nIDEvent) {
    case 69: {
      SmtIAToolManager* pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
      SmtBaseTool* pTmpTool =
          dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
      if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
        pTmpTool->Timer();
      }
      if (m_pFlashTool) {
        m_pFlashTool->Timer();
      }
    } break;
    case 71: {
      KillTimer(71);
      if (m_pSmtOperMap && !m_oper_map_frame.framed &&
          !m_oper_map_frame.framing) {
        LOGGING(LOG_INFO, "OnTimer(71): deferred frame_oper_map");
        frame_oper_map(/*realtime=*/true);
      }
    } break;
    case 70: {
      if (m_pRenderDevice && m_bActive) {
        if (m_pSmtOperMap && !m_oper_map_frame.framed &&
            !m_oper_map_frame.framing) {
          CRect client;
          GetClientRect(&client);
          if (client.Width() > 0 && client.Height() > 0) {
            LOGGING(LOG_INFO, "OnTimer: deferred frame_oper_map %dx%d",
                    client.Width(), client.Height());
            frame_oper_map(/*realtime=*/true);
          }
        }
        SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
        SmtSysPra sysPra = pSysMgr->get_sys_pra();
        Smt2DRenderOptions rdOptions;
        rdOptions.bShowMBR = sysPra.bShowMBR;
        rdOptions.bShowPoint = sysPra.bShowPoint;
        rdOptions.lPointRaduis = sysPra.lPointRaduis;
        m_pRenderDevice->SetRenderOptions(rdOptions);
        m_pRenderDevice->Timer();
      }
    } break;
    default:
      break;
  }

  SmtXView::OnTimer(nIDEvent);
}

void Smt2DXView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
  SmtIAToolManager* pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  SmtBaseTool* pTmpTool =
      dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
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
  POINT client = {pt.x, pt.y};
  ScreenToClient(&client);
  if (view_host()) {
    content::InputEvent e{};
    e.kind = content::InputEvent::Kind::kWheel;
    e.wheel = static_cast<int32_t>(zDelta);
    e.flags = static_cast<uint32_t>(nFlags);
    e.x_px = client.x;
    e.y_px = client.y;
    if (view_host()->dispatch_input(e)) {
      return TRUE;
    }
  }
  if (m_pViewCtrlTool) {
    m_pViewCtrlTool->MouseWeel(nFlags, zDelta, lPoint(pt.x, pt.y));
    return TRUE;
  }
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

BOOL Smt2DXView::OnEraseBkgnd(CDC* pDC) {
  (void)pDC;
  return TRUE;
}

void Smt2DXView::OnContextMenu(CWnd* pWnd, CPoint point) {
  (void)pWnd;
  SmtIAToolManager* pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  if (!pIAToolMgr) {
    return;
  }
  SmtBaseTool* pTmpTool =
      dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
  if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd) &&
      !pTmpTool->IsEnableContexMenu()) {
    return;
  }
  if (!m_hContexMenu || ::GetMenuItemCount(m_hContexMenu) <= 0) {
    return;
  }
  PostMessage(WM_APP + 0x4354, 0, MAKELPARAM(point.x, point.y));
}

LRESULT Smt2DXView::OnDeferredContextMenu(WPARAM, LPARAM lParam) {
  if (!m_hContexMenu || !::IsMenu(m_hContexMenu) ||
      ::GetMenuItemCount(m_hContexMenu) <= 0) {
    return 0;
  }
  CPoint point(static_cast<int>(static_cast<short>(LOWORD(lParam))),
               static_cast<int>(static_cast<short>(HIWORD(lParam))));
  CMenu contexMenu;
  if (!contexMenu.Attach(m_hContexMenu)) {
    return 0;
  }
  contexMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                            point.x, point.y, this);
  contexMenu.Detach();
  return 0;
}

BOOL Smt2DXView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) {
  SmtIAToolManager* pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  if (pIAToolMgr) {
    SmtBaseTool* pTmpTool =
        dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
    if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
      return pTmpTool->SetCursor();
    }
  }
  return SmtXView::OnSetCursor(pWnd, nHitTest, message);
}

bool Smt2DXView::InitCreate(void) { return SmtXView::InitCreate(); }

bool Smt2DXView::EndDestory(void) {
  SmtGroupToolFactory::DestoryGroupTool(m_pViewCtrlTool);
  SmtGroupToolFactory::DestoryGroupTool(m_pSelectTool);
  SmtGroupToolFactory::DestoryGroupTool(m_pFlashTool);
  SMT_SAFE_DELETE(m_pRenderer);
  m_pRenderDevice = nullptr;
  return SmtXView::EndDestory();
}

bool Smt2DXView::CreateContexMenu() {
  SmtXView::CreateContexMenu();
  append_listener_menu(m_hContexMenu, m_pViewCtrlTool, FIM_2DVIEW, false);
  append_listener_menu(m_hContexMenu, m_pFlashTool, FIM_2DVIEW);
  append_listener_menu(m_hContexMenu, m_pSelectTool, FIM_2DVIEW);
  detail::append_am_module_menus(m_hContexMenu, FIM_2DVIEW,
                                 /*insert_by_position=*/true);
  LOGGING(LOG_INFO, "Init 2DView ContexMenu OK!");
  return true;
}

bool Smt2DXView::CreateMainMenu() {
  SmtXView::CreateMainMenu();
  attach_listener_popup(m_hMainMenu, m_pViewCtrlTool, FIM_2DMFMENU,
                        m_pViewCtrlTool->get_name());
  attach_listener_popup(m_hMainMenu, m_pFlashTool, FIM_2DMFMENU,
                        m_pFlashTool->get_name());
  attach_listener_popup(m_hMainMenu, m_pSelectTool, FIM_2DMFMENU,
                        m_pSelectTool->get_name());
  detail::append_am_module_menus(m_hMainMenu, FIM_2DMFMENU,
                                 /*insert_by_position=*/false);
  LOGGING(LOG_INFO, "Init 2DView MainMenu OK!");
  return true;
}

bool Smt2DXView::CreateRender(void) {
  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtSysPra sysPra = pSysMgr->get_sys_pra();

  string strMapRenderDevice = sysPra.str2DRenderDeviceName;
  m_pRenderer = new SmtRenderer(AfxGetInstanceHandle());
  if (m_pRenderer->CreateDevice(strMapRenderDevice.c_str()) == SMT_ERR_FAILURE) {
    return false;
  }

  m_pRenderDevice = m_pRenderer->GetDevice();
  if (m_pRenderDevice == NULL) {
    return false;
  }

  if (m_pRenderDevice->Init(m_hWnd, strMapRenderDevice.c_str()) ==
      SMT_ERR_FAILURE) {
    LOGGING(LOG_INFO, "Init %s failure!", strMapRenderDevice.c_str());
    return false;
  }

  m_pRenderDevice->SetMapMode(MM_TEXT);

  {
    CString title = _T("二维 · Legacy Map2D (GDI+)");
    if (CDocument* doc = GetDocument()) {
      doc->SetTitle(title);
    }
    SetWindowText(title);
  }

  LOGGING(LOG_INFO, "Init %s ok!", strMapRenderDevice.c_str());
  return true;
}

bool Smt2DXView::CreateTools(void) {
  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();

  SmtGroupToolFactory::CreateGroupTool(m_pViewCtrlTool,
                                       GroupToolType::GTT_ViewControl);
  SmtGroupToolFactory::CreateGroupTool(m_pSelectTool,
                                       GroupToolType::GTT_Select);
  SmtGroupToolFactory::CreateGroupTool(m_pFlashTool, GroupToolType::GTT_Flash);

  if (NULL == m_pViewCtrlTool || NULL == m_pSelectTool ||
      NULL == m_pFlashTool) {
    LOGGING(LOG_INFO, "Init GroupTools failure!");
    return false;
  }

  SmtStyleConfig styleConfig = pSysMgr->get_sys_style_config();

  m_pViewCtrlTool->SetToolStyleName(styleConfig.szAuxStyle);
  if (m_pViewCtrlTool->Init(m_pRenderDevice, m_pSmtOperMap, m_hWnd) !=
      SMT_ERR_NONE) {
    return false;
  }

  m_pSelectTool->SetToolStyleName(styleConfig.szAuxStyle);
  if (m_pSelectTool->Init(m_pRenderDevice, m_pSmtOperMap, m_hWnd) !=
      SMT_ERR_NONE) {
    return false;
  }

  m_pFlashTool->SetToolStyleName(styleConfig.szAuxStyle);
  if (m_pFlashTool->Init(m_pRenderDevice, m_pSmtOperMap, m_hWnd) !=
      SMT_ERR_NONE) {
    return false;
  }

  m_pViewCtrlTool->SetActive();

  if (!view_host()) {
    reset_view_host(new content::ViewHost());
  }
  tool::Workspace* ws = view_host() ? view_host()->workspace() : NULL;
  detail::bind_2d_tools_workspace(m_pViewCtrlTool, m_pSelectTool, m_pFlashTool,
                                  /*append=*/nullptr, ws, m_hWnd);

  LOGGING(LOG_INFO, "Init GroupTools ok!");
  return true;
}

bool Smt2DXView::frame_oper_map(bool realtime) {
  return detail::frame_oper_map(m_hWnd, m_pRenderDevice, m_pSmtOperMap,
                                &m_oper_map_frame, realtime);
}

void Smt2DXView::request_oper_map_frame() {
  detail::request_oper_map_frame(m_hWnd, m_pSmtOperMap, m_oper_map_frame);
}

LRESULT Smt2DXView::OnFrameOperMap(WPARAM, LPARAM) {
  if (!m_oper_map_frame.framed && !m_oper_map_frame.framing) {
    LOGGING(LOG_INFO, "OnFrameOperMap: framing");
    frame_oper_map(/*realtime=*/true);
  }
  return 0;
}

void Smt2DXView::SetOperMap(SmtMap* pSmtMap) {
  m_pSmtOperMap = pSmtMap;
  m_oper_map_frame.framed = false;

  if (m_pViewCtrlTool) {
    m_pViewCtrlTool->SetOperMap(m_pSmtOperMap);
  }
  if (m_pSelectTool) {
    m_pSelectTool->SetOperMap(m_pSmtOperMap);
  }
  if (m_pFlashTool) {
    m_pFlashTool->SetOperMap(m_pSmtOperMap);
  }

  request_oper_map_frame();

  if (m_uiRefreshTimer == 0 || m_uiNotifyTimer == 0) {
    SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
    if (pSysMgr) {
      SmtSysPra sysPra = pSysMgr->get_sys_pra();
      if (m_uiRefreshTimer == 0) {
        m_uiRefreshTimer = SetTimer(69, sysPra.l2DViewRefreshTime, 0);
      }
      if (m_uiNotifyTimer == 0) {
        m_uiNotifyTimer = SetTimer(70, sysPra.l2DViewNotifyTime, 0);
      }
    }
  }
  Invalidate(FALSE);
}

BOOL Smt2DXView::OnCommand(WPARAM wParam, LPARAM lParam) {
  unsigned int unMsg = (wParam);
  if (unMsg >= SMT_MSG_CMD_BEGIN && unMsg <= SMT_MSG_CMD_END) {
    dispatch_menu_command(unMsg);
  } else if (unMsg >= SMT_MSG_USER_BEGIN && unMsg <= SMT_MSG_USER_END) {
    dispatch_menu_command(unMsg);
  }
  return SmtXView::OnCommand(wParam, lParam);
}

void Smt2DXView::apply_workspace_draft(const tool::Draft& draft) {
  if (draft.kind == tool::DraftKind::kWheel) {
    if (m_pViewCtrlTool) {
      m_pViewCtrlTool->apply_draft(draft);
    }
    return;
  }
  const char* tool_id = nullptr;
  if (view_host() && view_host()->workspace()) {
    if (tool::Interaction* cur = view_host()->workspace()->stack().current()) {
      tool_id = cur->id();
    }
  }
  if (draft.kind == tool::DraftKind::kRect && m_pViewCtrlTool &&
      (tool::draft_flags::is_touch_pan(draft.flags) ||
       tool::is_navigate_tool(tool_id))) {
    m_pViewCtrlTool->apply_draft(draft);
    return;
  }
  if (m_pSelectTool && tool_id && std::strncmp(tool_id, "select.", 7) == 0) {
    m_pSelectTool->apply_draft(draft);
    return;
  }
  SmtIAToolManager* mgr = SmtIAToolManager::get_singleton_ptr();
  SmtBaseTool* tool =
      mgr ? dynamic_cast<SmtBaseTool*>(mgr->GetActiveIATool()) : NULL;
  if (tool && tool->GetOwnerWnd() == m_hWnd) {
    tool->apply_draft(draft);
  }
}

}  // namespace ui
