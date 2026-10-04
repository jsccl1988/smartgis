#include "legacy/app/stdafx.h"

#include "legacy/app/views/viewport/data_view.h"

#include "base/core/log.h"
#include "legacy/app/views/document/document.h"
#include "legacy/app/shell/frame/win_app.h"
#include "legacy/app/views/helper/mdi_menu.h"
#include "legacy/ui/catalog/map/mapmgr.h"

using namespace gis;
using namespace ui;

IMPLEMENT_DYNCREATE(CSmartDataSourceView, Smt2DXView)

CSmartDataSourceView::CSmartDataSourceView() {}

CSmartDataSourceView::~CSmartDataSourceView() {}

BEGIN_MESSAGE_MAP(CSmartDataSourceView, Smt2DXView)
ON_WM_CREATE()
ON_WM_DESTROY()
END_MESSAGE_MAP()

void CSmartDataSourceView::OnDraw(CDC* pDC) {
  CDocument* pDoc = GetDocument();
  (void)pDoc;
  Smt2DXView::OnDraw(pDC);
}

#ifdef _DEBUG
void CSmartDataSourceView::AssertValid() const { Smt2DXView::AssertValid(); }

#ifndef _WIN32_WCE
void CSmartDataSourceView::Dump(CDumpContext& dc) const {
  Smt2DXView::Dump(dc);
}
#endif

CSmartGisDoc* CSmartDataSourceView::GetDocument() const {
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
  return (CSmartGisDoc*)m_pDocument;
}

#endif  //_DEBUG

int CSmartDataSourceView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (Smt2DXView::OnCreate(lpCreateStruct) == -1) return -1;
  return 0;
}

void CSmartDataSourceView::OnDestroy() {
  Smt2DXView::OnDestroy();

  SmtMapMgr* pMapMgr = SmtMapMgr::get_singleton_ptr();
  pMapMgr->Unregister2DXView((void*)this);
}

void CSmartDataSourceView::OnInitialUpdate() {
  LOGGING(LOG_INFO, "OnInitialUpdate begin");
  Smt2DXView::OnInitialUpdate();

  legacy_app::helper::attach_mdi_view_menu(m_hMainMenu, GetDocument());

  SmtMapMgr* pMapMgr = SmtMapMgr::get_singleton_ptr();
  Map* pSmtMap = pMapMgr->GetSmtMapPtr();
  pMapMgr->Register2DXView((void*)this);

  if (NULL != pSmtMap) {
    SetOperMap(pSmtMap);
  }
}

int CSmartDataSourceView::Notify(long nMsg, SmtListenerMsg& param) {
  switch (nMsg) {
    case SMT_MSG_GET_SYS_2DVIEW:
      *(Smt2DXView**)(param.lParam) = (Smt2DXView*)this;
      break;
  }
  return SMT_ERR_NONE;
}
