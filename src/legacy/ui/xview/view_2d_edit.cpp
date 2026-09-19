// Smt2DEditXView.cpp : 实锟斤拷锟侥硷拷
//

#include "stdafx.h"
#include "legacy/ui/xview/view_2d_edit.h"

#include "base/core/api.h"
#include "base/core/core.h"
#include "base/core/log.h"
#include "content/public/view_host.h"
#include "legacy/tool/group/appendfeaturetool.h"
#include "legacy/tool/group/defs.h"
#include "legacy/tool/group/flashtool.h"
#include "legacy/tool/group/selecttool.h"
#include "legacy/tool/group/viewctrltool.h"
#include "legacy/ui/xview/view_core.h"
#include "base/carto/stylemanager.h"
#include "gis/edit/map_edit_session.h"
#include "gis/feature/feature.h"
#include "gis/map/map.h"
#include "sys/sysmanager.h"

using namespace base;
using namespace geo;
using namespace gis;
using namespace render;
using namespace tool;
using namespace sys;

// Smt2DEditXView

namespace ui {
IMPLEMENT_DYNCREATE(Smt2DEditXView, Smt2DXView)

Smt2DEditXView::Smt2DEditXView() {
  m_pAppendFeaTool = NULL;
  m_pMapEdits = NULL;
}

Smt2DEditXView::~Smt2DEditXView() { ; }

BEGIN_MESSAGE_MAP(Smt2DEditXView, CView)
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
ON_WM_ERASEBKGND()
ON_WM_SETCURSOR()

END_MESSAGE_MAP()

// Smt2DEditXView 锟斤拷图

void Smt2DEditXView::OnDraw(CDC* pDC) {
  CDocument* pDoc = GetDocument();
  // TODO: 锟节达拷锟斤拷锟接伙拷锟狡达拷锟斤拷
  Smt2DXView::OnDraw(pDC);
}

void Smt2DEditXView::OnSize(UINT nType, int cx, int cy) {
  Smt2DXView::OnSize(nType, cx, cy);
}

// Smt2DEditXView 锟斤拷锟?
#ifdef _DEBUG
void Smt2DEditXView::AssertValid() const { CView::AssertValid(); }

#ifndef _WIN32_WCE
void Smt2DEditXView::Dump(CDumpContext& dc) const { CView::Dump(dc); }
#endif
#endif  //_DEBUG

// Smt2DEditXView 锟斤拷息锟斤拷锟斤拷锟斤拷锟斤拷
//////////////////////////////////////////////////////////////////////////
void Smt2DEditXView::OnMouseMove(UINT nFlags, CPoint point) {
  // TODO: restored after encoding merge
  Smt2DXView::OnMouseMove(nFlags, point);
}

void Smt2DEditXView::OnTimer(UINT_PTR nIDEvent) {
  // TODO: restored after encoding merge
  Smt2DXView::OnTimer(nIDEvent);
}

void Smt2DEditXView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
  // TODO: restored after encoding merge
  Smt2DXView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void Smt2DEditXView::OnLButtonDown(UINT nFlags, CPoint point) {
  // TODO: restored after encoding merge
  Smt2DXView::OnLButtonDown(nFlags, point);
}

void Smt2DEditXView::OnLButtonUp(UINT nFlags, CPoint point) {
  // TODO: restored after encoding merge
  Smt2DXView::OnLButtonUp(nFlags, point);
}

void Smt2DEditXView::OnRButtonDown(UINT nFlags, CPoint point) {
  // TODO: restored after encoding merge
  Smt2DXView::OnRButtonDown(nFlags, point);
}

BOOL Smt2DEditXView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
  // TODO: restored after encoding merge
  return Smt2DXView::OnMouseWheel(nFlags, zDelta, pt);
}

void Smt2DEditXView::OnLButtonDblClk(UINT nFlags, CPoint point) {
  // TODO: restored after encoding merge
  Smt2DXView::OnLButtonDblClk(nFlags, point);
}

void Smt2DEditXView::OnRButtonDblClk(UINT nFlags, CPoint point) {
  // TODO: restored after encoding merge
  Smt2DXView::OnRButtonDblClk(nFlags, point);
}

void Smt2DEditXView::OnRButtonUp(UINT nFlags, CPoint point) {
  // TODO: restored after encoding merge
  Smt2DXView::OnRButtonUp(nFlags, point);
}

BOOL Smt2DEditXView::OnEraseBkgnd(CDC* pDC) {
  // TODO: restored after encoding merge
  return Smt2DXView::OnEraseBkgnd(pDC);
}

void Smt2DEditXView::OnContextMenu(CWnd* pWnd, CPoint point) {
  // TODO: restored after encoding merge
  Smt2DXView::OnContextMenu(pWnd, point);
}

BOOL Smt2DEditXView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) {
  return Smt2DXView::OnSetCursor(pWnd, nHitTest, message);
}

//////////////////////////////////////////////////////////////////////////
bool Smt2DEditXView::InitCreate(void) { return Smt2DXView::InitCreate(); }

bool Smt2DEditXView::EndDestory(void) {
  SmtGroupToolFactory::DestoryGroupTool(m_pAppendFeaTool);
  const bool ok = Smt2DXView::EndDestory();
  delete m_pMapEdits;
  m_pMapEdits = NULL;
  return ok;
}

bool Smt2DEditXView::CreateTools(void) {
  Smt2DXView::CreateTools();
  // sys style
  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

  // 锟斤拷锟斤拷tool
  SmtGroupToolFactory::CreateGroupTool(m_pAppendFeaTool,
                                       GroupToolType::GTT_AppendFeature);

  if (NULL == m_pAppendFeaTool) {
    return false;
  }

  // 锟斤拷锟斤拷tool
  m_pAppendFeaTool->SetToolStyleName(styleSonfig.szAuxStyle);

  if (m_pAppendFeaTool->Init(m_pRenderDevice, m_pSmtOperMap, m_hWnd) !=
      SMT_ERR_NONE)
    return false;

  delete m_pMapEdits;
  m_pMapEdits = new gis::MapEditSession(m_pSmtOperMap);
  reset_view_host(new content::ViewHost(m_pMapEdits));
  // Parent CreateTools bound tools to the prior host; rebind after reset.
  tool::Workspace* ws = view_host() ? view_host()->workspace() : NULL;
  if (SmtViewCtrlTool* view_ctrl =
          dynamic_cast<SmtViewCtrlTool*>(m_pViewCtrlTool)) {
    view_ctrl->bind_workspace(ws);
  }
  if (SmtSelectTool* select = dynamic_cast<SmtSelectTool*>(m_pSelectTool)) {
    select->bind_workspace(ws);
  }
  if (SmtFlashTool* flash = dynamic_cast<SmtFlashTool*>(m_pFlashTool)) {
    flash->bind_workspace(ws);
  }
  if (SmtAppendFeatureTool* append =
          dynamic_cast<SmtAppendFeatureTool*>(m_pAppendFeaTool)) {
    append->bind_workspace(ws);
  }

  LOGGING(LOG_INFO, "Init 2DEditView GroupTools OK!");

  return true;
}

void Smt2DEditXView::SetOperMap(SmtMap* pSmtMap) {
  Smt2DXView::SetOperMap(pSmtMap);

  if (m_pAppendFeaTool) m_pAppendFeaTool->SetOperMap(pSmtMap);

  if (m_pMapEdits) m_pMapEdits->bind_map(pSmtMap);
}

bool Smt2DEditXView::CreateContexMenu() {
  Smt2DXView::CreateContexMenu();
  // append feature menu
  append_listener_menu(m_hContexMenu, m_pAppendFeaTool, FIM_2DVIEW);

  LOGGING(LOG_INFO, "Init 2DEditView ContexMenu OK!");

  return true;
}

bool Smt2DEditXView::CreateMainMenu() {
  Smt2DXView::CreateMainMenu();
  attach_listener_popup(m_hMainMenu, m_pAppendFeaTool, FIM_2DMFMENU,
                        m_pAppendFeaTool->get_name(), 0, MF_BYPOSITION);

  LOGGING(LOG_INFO, "Init 2DEditView MainMenu OK!");

  return true;
}
}  // namespace ui