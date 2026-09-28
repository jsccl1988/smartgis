#include "legacy/app/stdafx.h"

#include "legacy/app/view/smart_gis_view.h"

#include "legacy/app/doc/smart_gis_doc.h"
#include "legacy/app/shell/main_frame.h"
#include "legacy/app/shell/smart_gis.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


IMPLEMENT_DYNCREATE(CSmartGisView, CView)

BEGIN_MESSAGE_MAP(CSmartGisView, CView)
ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CView::OnFilePrintPreview)

ON_WM_CREATE()
ON_WM_DESTROY()
ON_WM_SIZE()

ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_RBUTTONDOWN()
ON_WM_SETCURSOR()
ON_WM_ERASEBKGND()
ON_WM_MOUSEWHEEL()
ON_WM_KEYDOWN()
ON_WM_CONTEXTMENU()

END_MESSAGE_MAP()


CSmartGisView::CSmartGisView() {
}

CSmartGisView::~CSmartGisView() {}

BOOL CSmartGisView::PreCreateWindow(CREATESTRUCT& cs) {

  return CView::PreCreateWindow(cs);
}


void CSmartGisView::OnDraw(CDC* pDC) {
  CSmartGisDoc* pDoc = GetDocument();
  ASSERT_VALID(pDoc);
  if (!pDoc) return;
}

BOOL CSmartGisView::OnPreparePrinting(CPrintInfo* pInfo) {
  return DoPreparePrinting(pInfo);
}

void CSmartGisView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/) {
}

void CSmartGisView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/) {
}


#ifdef _DEBUG
void CSmartGisView::AssertValid() const { CView::AssertValid(); }

void CSmartGisView::Dump(CDumpContext& dc) const { CView::Dump(dc); }

CSmartGisDoc* CSmartGisView::GetDocument()
    const
{
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
  return (CSmartGisDoc*)m_pDocument;
}
#endif  //_DEBUG


void CSmartGisView::OnInitialUpdate() { CView::OnInitialUpdate(); }

int CSmartGisView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CView::OnCreate(lpCreateStruct) == -1) return -1;

  return 0;
}

void CSmartGisView::OnDestroy() {
  CView::OnDestroy();

}

void CSmartGisView::OnSize(UINT nType, int cx, int cy) {
  CView::OnSize(nType, cx, cy);

}

void CSmartGisView::OnLButtonDown(UINT nFlags, CPoint point) {
  // Pass the message along
  CView::OnLButtonDown(nFlags, point);
}

void CSmartGisView::OnMouseMove(UINT nFlags, CPoint point) {
  // Pass the message along
  CView::OnMouseMove(nFlags, point);
}

void CSmartGisView::OnLButtonUp(UINT nFlags, CPoint point) {
  // Pass the message along
  CView::OnLButtonUp(nFlags, point);
}

void CSmartGisView::OnRButtonDown(UINT nFlags, CPoint point) {
  CView::OnRButtonDown(nFlags, point);
}

BOOL CSmartGisView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {

  return CView::OnMouseWheel(nFlags, zDelta, pt);
}

void CSmartGisView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
  CView::OnKeyDown(nChar, nRepCnt, nFlags);
}
BOOL CSmartGisView::OnEraseBkgnd(CDC* pDC) {

  return TRUE;
}

void CSmartGisView::OnContextMenu(CWnd* pWnd, CPoint point) {
}

BOOL CSmartGisView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) {
  return TRUE;
}

void CSmartGisView::OnPrepareDC(CDC* pDC, CPrintInfo* pInfo) {

  CView::OnPrepareDC(pDC, pInfo);
}
