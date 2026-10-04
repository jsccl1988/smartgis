#include "legacy/app/stdafx.h"

#include "legacy/app/views/viewport/edit_view.h"

#include "base/core/log.h"
#include "legacy/app/views/document/document.h"
#include "legacy/app/shell/frame/win_app.h"
#include "legacy/app/views/helper/mdi_menu.h"
#include "legacy/app/views/helper/self_test_mark.h"
#include "legacy/app/views/helper/status_coord.h"
#include "legacy/ui/catalog/map/mapmgr.h"

using namespace gis;

IMPLEMENT_DYNCREATE(CSmartMapEditView, Smt2DEditXView)

CSmartMapEditView::CSmartMapEditView() {}

CSmartMapEditView::~CSmartMapEditView() {}

BEGIN_MESSAGE_MAP(CSmartMapEditView, Smt2DEditXView)
ON_WM_CREATE()
ON_WM_DESTROY()
ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

void CSmartMapEditView::OnDraw(CDC* pDC) {
  CDocument* pDoc = GetDocument();
  (void)pDoc;
  Smt2DEditXView::OnDraw(pDC);
}

#ifdef _DEBUG
void CSmartMapEditView::AssertValid() const { Smt2DEditXView::AssertValid(); }

#ifndef _WIN32_WCE
void CSmartMapEditView::Dump(CDumpContext& dc) const {
  Smt2DEditXView::Dump(dc);
}
#endif

CSmartGisDoc* CSmartMapEditView::GetDocument() const {
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
  return (CSmartGisDoc*)m_pDocument;
}

#endif  //_DEBUG

void CSmartMapEditView::OnMouseMove(UINT nFlags, CPoint point) {
  Smt2DEditXView::OnMouseMove(nFlags, point);

  float x = 0, y = 0;
  if (m_pRenderDevice) {
    m_pRenderDevice->DPToLP(point.x, point.y, x, y);
  }
  legacy_app::helper::set_status_map_xy(x, y);
}

void CSmartMapEditView::OnInitialUpdate() {
  LOGGING(LOG_INFO, "OnInitialUpdate begin");
  Smt2DEditXView::OnInitialUpdate();
  LOGGING(LOG_INFO, "OnInitialUpdate after base");

  legacy_app::helper::attach_mdi_view_menu(m_hMainMenu, GetDocument());
  LOGGING(LOG_INFO, "OnInitialUpdate menu ok");

  // Attach the bootstrapped map (china_city) for EDIT1 paint.
  SmtMapMgr* pMapMgr = SmtMapMgr::get_singleton_ptr();
  Map* pSmtMap = pMapMgr->GetSmtMapPtr();
  pMapMgr->Register2DXView((void*)this);

  if (NULL != pSmtMap) {
    LOGGING(LOG_INFO, "OnInitialUpdate SetOperMap begin");
    SetOperMap(pSmtMap);
    LOGGING(LOG_INFO, "OnInitialUpdate SetOperMap done");
  }
}

int CSmartMapEditView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  LOGGING(LOG_INFO, "CSmartMapEditView::OnCreate enter");
  if (Smt2DEditXView::OnCreate(lpCreateStruct) == -1) return -1;

  LOGGING(LOG_INFO, "CSmartMapEditView::OnCreate leave");
  legacy_app::helper::arm_self_test_view_watchdog_if_requested();
  return 0;
}

void CSmartMapEditView::OnDestroy() {
  Smt2DEditXView::OnDestroy();

  SmtMapMgr* pMapMgr = SmtMapMgr::get_singleton_ptr();
  pMapMgr->Unregister2DXView((void*)this);
}

void CSmartMapEditView::OnActivate(UINT nState, CWnd* pWndOther,
                                   BOOL bMinimized) {
  Smt2DEditXView::OnActivate(nState, pWndOther, bMinimized);
}

void CSmartMapEditView::OnActivateApp(BOOL bActive, DWORD dwThreadID) {
  Smt2DEditXView::OnActivateApp(bActive, dwThreadID);
}

int CSmartMapEditView::Notify(long nMsg, SmtListenerMsg& param) {
  switch (nMsg) {
    case SMT_MSG_GET_SYS_2DEDITVIEW:
      *(Smt2DEditXView**)(param.lParam) = (Smt2DEditXView*)this;
      break;
  }
  return SMT_ERR_NONE;
}
