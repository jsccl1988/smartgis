// Smt2DXView.cpp : 实锟斤拷锟侥硷拷
//

#include "stdafx.h"
#include "legacy/ui/xview/view_2d.h"

#include "base/core/core.h"
#include "base/core/log.h"
#include "legacy/ui/xview/view_core.h"
#include "base/carto/stylemanager.h"
#include "sys/sysmanager.h"
namespace gis {
class SmtFeature;
}
#include <algorithm>
#include <cstring>
#include <vector>

#include "base/core/api.h"
#include "base/core/listenermanager.h"
#include "content/public/view_host.h"
#include "legacy/tool/group/defs.h"
#include "legacy/tool/group/flashtool.h"
#include "legacy/tool/group/selecttool.h"
#include "legacy/tool/group/viewctrltool.h"
#include "legacy/tool/t_iatoolmanager.h"
#include "legacy/ui/xcatalog/mapmgr.h"
#include "plugin/legacy/module_manager.h"
#include "plugin/legacy/plugin_msg.h"
#include "base/carto/style_api.h"
#include "gis/feature/feature.h"
#include "gis/map/map.h"
#include "tool/workspace.h"

using namespace base;
using namespace geo;
using namespace gis;
using namespace render;
using namespace tool;
using namespace sys;
using namespace plugin;

namespace {

// Rubber-band is drawn AFTER RenderMap onto the window DC (MRD_BL_DIRECT).
// Persisting strokes in MRD_BL_QUICK stacked every MouseMove when ClearBuf was
// a no-op; Present-then-overlay keeps a single live rectangle.
void paint_aux_overlay(LPRENDERDEVICE device, const tool::AuxOverlay *overlay) {
  if (!device || !overlay || overlay->points.size() < 2 ||
      overlay->kind == tool::AuxOverlay::Kind::kNone) {
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
      device->BeginRender(MRD_BL_DIRECT, false, style, R2_COPYPEN)) {
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
  device->EndRender(MRD_BL_DIRECT);
}

}  // namespace

// Smt2DXView

namespace ui {
IMPLEMENT_DYNCREATE(Smt2DXView, SmtXView)

// Posted after SetOperMap so framing runs outside OnInitialUpdate / nested pumps.
#define SMT_MSG_FRAME_OPER_MAP (WM_APP + 0x2D01)

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
  m_bOperMapFramed = false;
  m_bFramingOperMap = false;
  m_uiRefreshTimer = 0;
  m_uiNotifyTimer = 0;
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
ON_MESSAGE(SMT_MSG_FRAME_OPER_MAP, OnFrameOperMap)

END_MESSAGE_MAP()

// Smt2DXView 锟斤拷图

void Smt2DXView::OnDraw(CDC *pDC) {
  CDocument *pDoc = GetDocument();
  (void)pDoc;
  /*if (!m_bActive)
  return;*/

  // Wipe QUICK, Present the map, then flash + rubber-band on the window DC.
  // Flash used to paint DYNAMIC before Present and was composited away / empty.
  if (m_pRenderDevice) {
    if (SMT_ERR_NONE ==
        m_pRenderDevice->BeginRender(MRD_BL_QUICK, true, nullptr, R2_COPYPEN)) {
      m_pRenderDevice->EndRender(MRD_BL_QUICK);
    }
    m_pRenderDevice->RenderMap();
  }

  if (m_pFlashTool) m_pFlashTool->AuxDraw();

  if (view_host() && view_host()->workspace()) {
    view_host()->workspace()->aux_draw();
    paint_aux_overlay(m_pRenderDevice,
                      view_host()->workspace()->live_preview());
  }
}

// Smt2DXView 锟斤拷锟?
#ifdef _DEBUG
void Smt2DXView::AssertValid() const { SmtXView::AssertValid(); }

#ifndef _WIN32_WCE
void Smt2DXView::Dump(CDumpContext &dc) const { SmtXView::Dump(dc); }
#endif
#endif  //_DEBUG

// Smt2DXView 锟斤拷息锟斤拷锟斤拷锟斤拷锟斤拷

int Smt2DXView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (SmtXView::OnCreate(lpCreateStruct) == -1) return -1;

  LOGGING(LOG_INFO, "Smt2DXView::OnCreate after InitCreate (timers deferred)");
  // Defer SetTimer until SetOperMap / OnInitialUpdate. Starting timers while
  // m_bActive defaults TRUE races OpenDocumentFile's nested message pump and
  // has caused STATUS_FATAL_APP_EXIT before OnInitialUpdate.
  return 0;
}

void Smt2DXView::OnDestroy() {
  // Stop refresh timers before EndDestory tears down the render device.
  KillTimer(69);
  KillTimer(70);
  SmtXView::OnDestroy();
}

void Smt2DXView::OnSize(UINT nType, int cx, int cy) {
  SmtXView::OnSize(nType, cx, cy);

  // frame_oper_map already Resize+ZoomToRect; a nested WM_SIZE must not
  // wipe buffers mid-paint.
  if (m_bFramingOperMap) {
    return;
  }

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
    // Prefer posted framing over sync ZoomToRect during layout.
    if (m_pSmtOperMap && !m_bOperMapFramed) {
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
    case 71: {
      KillTimer(71);
      if (m_pSmtOperMap && !m_bOperMapFramed && !m_bFramingOperMap) {
        LOGGING(LOG_INFO, "OnTimer(71): deferred frame_oper_map");
        frame_oper_map(/*realtime=*/true);
      }
    } break;
    case 70: {
      if (m_pRenderDevice && m_bActive) {
        // Catch the deferred-EDIT case: SetOperMap ran at 0x0 client and no
        // later WM_SIZE / posted frame arrived with a positive size.
        if (m_pSmtOperMap && !m_bOperMapFramed && !m_bFramingOperMap) {
          CRect client;
          GetClientRect(&client);
          if (client.Width() > 0 && client.Height() > 0) {
            LOGGING(LOG_INFO, "OnTimer: deferred frame_oper_map %dx%d",
                    client.Width(), client.Height());
            frame_oper_map(/*realtime=*/true);
          }
        }
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
  (void)pDC;
  return FALSE;
}

void Smt2DXView::OnContextMenu(CWnd *pWnd, CPoint point) {
  (void)pWnd;
  SmtIAToolManager *pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
  SmtBaseTool *pTmpTool =
      dynamic_cast<SmtBaseTool *>(pIAToolMgr->GetActiveIATool());
  if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd)) {
    if (pTmpTool && !pTmpTool->IsEnableContexMenu()) return;
  }

  if (!m_hContexMenu || ::GetMenuItemCount(m_hContexMenu) <= 0) {
    return;
  }

  CMenu contexMenu;
  if (!contexMenu.Attach(m_hContexMenu)) {
    return;
  }
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
  // Device lived inside the renderer; clear the dangling alias.
  m_pRenderDevice = nullptr;

  return SmtXView::EndDestory();
}

bool Smt2DXView::CreateContexMenu() {
  SmtXView::CreateContexMenu();
  // view ctrl menu
  append_listener_menu(m_hContexMenu, m_pViewCtrlTool, FIM_2DVIEW, false);

  // flash menu
  append_listener_menu(m_hContexMenu, m_pFlashTool, FIM_2DVIEW);

  // select menu
  append_listener_menu(m_hContexMenu, m_pSelectTool, FIM_2DVIEW);

  //////////////////////////////////////////////////////////////////////////
  // am menu
  ::AppendMenuA(m_hContexMenu, MF_SEPARATOR, 0, NULL);
  SmtAModuleManager *pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
  if (pAModuleMgr) {
    for (int i = 0; i < pAModuleMgr->get_a_module_count(); i++) {
      SmtAuxModule *pAModule = pAModuleMgr->get_a_module(i);
      attach_listener_popup(m_hContexMenu, pAModule, FIM_2DVIEW,
                            pAModule->get_name(), i + 3, MF_BYPOSITION);
    }
  }

  LOGGING(LOG_INFO, "Init 2DView ContexMenu OK!");

  return true;
}

bool Smt2DXView::CreateMainMenu() {
  SmtXView::CreateMainMenu();
  // view ctrl menu
  attach_listener_popup(m_hMainMenu, m_pViewCtrlTool, FIM_2DMFMENU,
                        m_pViewCtrlTool->get_name());

  // flash menu
  attach_listener_popup(m_hMainMenu, m_pFlashTool, FIM_2DMFMENU,
                        m_pFlashTool->get_name());

  // select menu
  attach_listener_popup(m_hMainMenu, m_pSelectTool, FIM_2DMFMENU,
                        m_pSelectTool->get_name());

  //////////////////////////////////////////////////////////////////////////
  // am menu
  ::AppendMenuA(m_hMainMenu, MF_SEPARATOR, 0, NULL);
  SmtAModuleManager *pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
  if (pAModuleMgr) {
    for (int i = 0; i < pAModuleMgr->get_a_module_count(); i++) {
      SmtAuxModule *pAModule = pAModuleMgr->get_a_module(i);
      attach_listener_popup(m_hMainMenu, pAModule, FIM_2DMFMENU,
                            pAModule->get_name());
    }
  }

  LOGGING(LOG_INFO, "Init 2DView MainMenu OK!");

  return true;
}

//////////////////////////////////////////////////////////////////////////
bool Smt2DXView::CreateRender(void) {
  SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtSysPra sysPra = pSysMgr->get_sys_pra();

  string strMapRenderDevice = sysPra.str2DRenderDeviceName;
  m_pRenderer = new SmtRenderer(AfxGetInstanceHandle());
  if (m_pRenderer->CreateDevice(strMapRenderDevice.c_str()) == SMT_ERR_FAILURE)
    return false;

  m_pRenderDevice = m_pRenderer->GetDevice();
  if (m_pRenderDevice == NULL) return false;

  if (m_pRenderDevice->Init(m_hWnd, strMapRenderDevice.c_str()) ==
      SMT_ERR_FAILURE) {
    LOGGING(LOG_INFO, "Init %s failure!", strMapRenderDevice.c_str());
    return false;
  }

  m_pRenderDevice->SetMapMode(MM_TEXT);

  LOGGING(LOG_INFO, "Init %s ok!", strMapRenderDevice.c_str());

  return true;
}

bool Smt2DXView::CreateTools(void) {
  SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();

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

  // Close SP1b bind window: browse 2D gets a host so ViewCtrl/Select/Flash
  // stay on Workspace pointers. Edit view may reset_view_host afterward.
  if (!view_host()) {
    reset_view_host(new content::ViewHost());
  }
  tool::Workspace *ws = view_host() ? view_host()->workspace() : NULL;
  if (SmtViewCtrlTool *view_ctrl =
          dynamic_cast<SmtViewCtrlTool *>(m_pViewCtrlTool)) {
    view_ctrl->bind_workspace(ws);
  }
  if (SmtSelectTool *select = dynamic_cast<SmtSelectTool *>(m_pSelectTool)) {
    select->bind_workspace(ws);
  }
  if (SmtFlashTool *flash = dynamic_cast<SmtFlashTool *>(m_pFlashTool)) {
    flash->bind_workspace(ws);
  }

  LOGGING(LOG_INFO, "Init GroupTools ok!");

  return true;
}

bool Smt2DXView::frame_oper_map(bool realtime) {
  if (!m_pRenderDevice || !m_pSmtOperMap) {
    return false;
  }
  if (m_bFramingOperMap) {
    LOGGING(LOG_INFO, "frame_oper_map: re-entrant skip");
    return false;
  }
  CRect client;
  GetClientRect(&client);
  const int cx = client.Width();
  const int cy = client.Height();
  if (cx <= 0 || cy <= 0) {
    LOGGING(LOG_INFO, "frame_oper_map: skip (client %dx%d)", cx, cy);
    return false;
  }

  m_bFramingOperMap = true;

  m_pRenderDevice->Resize(0, 0, cx, cy);

  Envelope env;
  m_pSmtOperMap->CalEnvelope();
  m_pSmtOperMap->get_envelope(env);
  bool framed = false;
  if (env.is_init()) {
    fRect frt;
    envelope_to_rect(frt, env);
    const float pad_x = (std::max)(frt.width() / 40.f, 0.01f);
    const float pad_y = (std::max)(frt.height() / 40.f, 0.01f);
    frt.rt.x += pad_x;
    frt.rt.y += pad_y;
    frt.lb.x -= pad_x;
    frt.lb.y -= pad_y;
    // ZoomToRect already paints when realtime; no second RefreshDirectly.
    const int zr = m_pRenderDevice->ZoomToRect(m_pSmtOperMap, frt, realtime);
    framed = (zr == SMT_ERR_NONE);
    LOGGING(LOG_INFO,
            "frame_oper_map: ZoomToRect rt=%d zr=%d env=(%.3f,%.3f)-(%.3f,%.3f)",
            realtime ? 1 : 0, zr, env.MinX, env.MinY, env.MaxX, env.MaxY);
  } else {
    lRect lrt;
    lrt.lb.x = 0;
    lrt.rt.y = 0;
    lrt.rt.x = cx;
    lrt.lb.y = cy;
    const int rr =
        m_pRenderDevice->RefreshDirectly(m_pSmtOperMap, lrt, realtime);
    framed = (rr == SMT_ERR_NONE);
    LOGGING(LOG_INFO, "frame_oper_map: empty envelope RefreshDirectly=%d", rr);
  }

  m_bOperMapFramed = framed;
  m_bFramingOperMap = false;
  return framed;
}

void Smt2DXView::request_oper_map_frame() {
  if (!m_pSmtOperMap || m_bOperMapFramed || m_bFramingOperMap) {
    return;
  }
  // Timer (not PostMessage): BCG OnInitialUpdate still nests a pump that
  // would run a posted frame mid-construction and AV in ZoomToRect.
  SetTimer(71, 50, nullptr);
}

LRESULT Smt2DXView::OnFrameOperMap(WPARAM, LPARAM) {
  if (!m_bOperMapFramed && !m_bFramingOperMap) {
    LOGGING(LOG_INFO, "OnFrameOperMap: framing");
    frame_oper_map(/*realtime=*/true);
  }
  return 0;
}

void Smt2DXView::SetOperMap(SmtMap *pSmtMap) {
  m_pSmtOperMap = pSmtMap;
  m_bOperMapFramed = false;

  if (m_pViewCtrlTool) m_pViewCtrlTool->SetOperMap(m_pSmtOperMap);

  if (m_pSelectTool) m_pSelectTool->SetOperMap(m_pSmtOperMap);

  if (m_pFlashTool) m_pFlashTool->SetOperMap(m_pSmtOperMap);

  // Do not ZoomToRect here: OnInitialUpdate / BCG nested pumps re-enter OnSize
  // and ACCESS_VIOLATION mid-paint. Frame via timer after the pump is idle.
  request_oper_map_frame();

  // Start refresh timers only after the map is attached.
  if (m_uiRefreshTimer == 0 || m_uiNotifyTimer == 0) {
    SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
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
  // TODO: restored after encoding merge
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
  // Workspace select.* must hit leftover SelectTool even if ViewCtrl is
  // still the active IA tool (point-select notify used to drop SetActive).
  if (m_pSelectTool && view_host() && view_host()->workspace()) {
    tool::Interaction *cur = view_host()->workspace()->stack().current();
    if (cur && cur->id() && std::strncmp(cur->id(), "select.", 7) == 0) {
      m_pSelectTool->apply_draft(draft);
      return;
    }
  }
  SmtIAToolManager *mgr = SmtIAToolManager::get_singleton_ptr();
  SmtBaseTool *tool =
      mgr ? dynamic_cast<SmtBaseTool *>(mgr->GetActiveIATool()) : NULL;
  if (tool && tool->GetOwnerWnd() == m_hWnd) {
    tool->apply_draft(draft);
  }
}
}  // namespace ui