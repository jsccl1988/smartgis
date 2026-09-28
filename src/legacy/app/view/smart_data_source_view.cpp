#include "legacy/app/stdafx.h"

#include "legacy/app/view/smart_data_source_view.h"

#include "base/core/log.h"
#include "legacy/app/doc/smart_gis_doc.h"
#include "legacy/app/shell/smart_gis.h"
#include "legacy/ui/xcatalog/mapmgr.h"

using namespace gis;
using namespace ui;

IMPLEMENT_DYNCREATE(CSmartDataSourceView, Smt2DXView)

CSmartDataSourceView::CSmartDataSourceView() {}

CSmartDataSourceView::~CSmartDataSourceView() {}

BEGIN_MESSAGE_MAP(CSmartDataSourceView, Smt2DXView)
ON_WM_CREATE()
ON_WM_DESTROY()
END_MESSAGE_MAP()


void CSmartDataSourceView::OnDraw(CDC *pDC) {
  CDocument *pDoc = GetDocument();
  Smt2DXView::OnDraw(pDC);
}


#ifdef _DEBUG
void CSmartDataSourceView::AssertValid() const { Smt2DXView::AssertValid(); }

#ifndef _WIN32_WCE
void CSmartDataSourceView::Dump(CDumpContext &dc) const {
  Smt2DXView::Dump(dc);
}
#endif

CSmartGisDoc *CSmartDataSourceView::GetDocument()
    const
{
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
  return (CSmartGisDoc *)m_pDocument;
}

#endif  //_DEBUG

int CSmartDataSourceView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (Smt2DXView::OnCreate(lpCreateStruct) == -1) return -1;


  return 0;
}

void CSmartDataSourceView::OnDestroy() {
  Smt2DXView::OnDestroy();

  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  pMapMgr->Unregister2DXView((void *)this);
}

void CSmartDataSourceView::OnInitialUpdate() {
  LOGGING(LOG_INFO, "OnInitialUpdate begin");
  Smt2DXView::OnInitialUpdate();

  theApp.append_mdi_window_menu(m_hMainMenu);
  ((CSmartGisDoc *)GetDocument())->m_hCurMainMenu = m_hMainMenu;
  ((CFrameWnd *)AfxGetMainWnd())->OnUpdateFrameMenu(NULL);
  AfxGetMainWnd()->DrawMenuBar();

  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  SmtMap *pSmtMap = pMapMgr->GetSmtMapPtr();
  pMapMgr->Register2DXView((void *)this);

  if (NULL != pSmtMap) {
    SetOperMap(pSmtMap);
  }
}

int CSmartDataSourceView::Notify(long nMsg, SmtListenerMsg &param) {
  switch (nMsg) {
    case SMT_MSG_GET_SYS_2DVIEW:
      *(Smt2DXView **)(param.lParam) = (Smt2DXView *)this;
      break;
  }
  return SMT_ERR_NONE;
}
