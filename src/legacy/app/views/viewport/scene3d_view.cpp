#include "legacy/app/stdafx.h"

#include "legacy/app/views/viewport/scene3d_view.h"

#include "base/core/log.h"
#include "legacy/app/views/document/document.h"
#include "legacy/app/shell/frame/win_app.h"
#include "legacy/app/views/helper/mdi_menu.h"
#include "legacy/app/views/helper/status_coord.h"
#include "legacy/ui/catalog/scene/scenemgr.h"

using namespace base;
using namespace ui;

IMPLEMENT_DYNCREATE(CSmart3DView, Smt3DXView)

CSmart3DView::CSmart3DView() {}

CSmart3DView::~CSmart3DView() {}

BEGIN_MESSAGE_MAP(CSmart3DView, Smt3DXView)
ON_WM_CREATE()
ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

void CSmart3DView::OnDraw(CDC* pDC) {
  CDocument* pDoc = GetDocument();
  (void)pDoc;
  Smt3DXView::OnDraw(pDC);
}

#ifdef _DEBUG
void CSmart3DView::AssertValid() const { Smt3DXView::AssertValid(); }

#ifndef _WIN32_WCE
void CSmart3DView::Dump(CDumpContext& dc) const { Smt3DXView::Dump(dc); }
#endif

CSmartGisDoc* CSmart3DView::GetDocument() const {
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
  return (CSmartGisDoc*)m_pDocument;
}

#endif  //_DEBUG

int CSmart3DView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (Smt3DXView::OnCreate(lpCreateStruct) == -1) return -1;
  return 0;
}

void CSmart3DView::OnMouseMove(UINT nFlags, CPoint point) {
  Smt3DXView::OnMouseMove(nFlags, point);
  legacy_app::helper::set_status_scene_xyz(m_vCursor3DPos.x, m_vCursor3DPos.y,
                                         m_vCursor3DPos.z);
}

void CSmart3DView::OnInitialUpdate() {
  LOGGING(LOG_INFO, "OnInitialUpdate begin");
  Smt3DXView::OnInitialUpdate();

  legacy_app::helper::attach_mdi_view_menu(m_hMainMenu, GetDocument());

  // Register this view with the scene manager.
  SmtSceneMgr* pSceneMgr = SmtSceneMgr::get_singleton_ptr();
  pSceneMgr->Register3DXView((void*)this);
  pSceneMgr->AttachScene(m_pScene);
}

int CSmart3DView::Notify(long nMsg, SmtListenerMsg& param) {
  switch (nMsg) {
    case SMT_MSG_GET_SYS_3DVIEW:
      *(Smt3DXView**)(param.lParam) = (Smt3DXView*)this;
      break;
  }

  return SMT_ERR_NONE;
}
