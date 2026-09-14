// SmartGisView.cpp : CSmartGisView 类뿯皽뿯宽뿯玽
//

#include "legacy_app/stdafx.h"
#include "legacy_app/smart_gis.h"

#include "legacy_app/smart_gis_doc.h"
#include "legacy_app/smart_gis_view.h"
#include "legacy_app/main_frame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CSmartGisView

IMPLEMENT_DYNCREATE(CSmartGisView, CView)

BEGIN_MESSAGE_MAP(CSmartGisView, CView)
	// 标뿯冽打印命뿯亽
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

// CSmartGisView 뿯枽붿/뿯枽뿯枽

CSmartGisView::CSmartGisView()
{
	// TODO: 在此处뿯涽뿯劽뿯枽붿뿯亽码
}

CSmartGisView::~CSmartGisView()
{

}

BOOL CSmartGisView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: 在此处붿过뿯侽改
	//  CREATESTRUCT cs 来뿯侽改뿯窽뿯厽类或样式

	return CView::PreCreateWindow(cs);
}

// CSmartGisView 뿯纽制

void CSmartGisView::OnDraw(CDC* pDC)
{
	CSmartGisDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;
	// TODO: 在此处为本机数据뿯涽뿯劽뿯纽制뿯亽码
}
// CSmartGisView 打印

BOOL CSmartGisView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// 默뿯붿뿯冽备
	return DoPreparePrinting(pInfo);
}

void CSmartGisView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 뿯涽뿯劽뿯붿外뿯皽打印前进붿뿯皽初뿯妽化过程
}

void CSmartGisView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 뿯涽뿯劽打印后进붿뿯皽清붿过程
}


// CSmartGisView 诊뿯施

#ifdef _DEBUG
void CSmartGisView::AssertValid() const
{
	CView::AssertValid();
}

void CSmartGisView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CSmartGisDoc* CSmartGisView::GetDocument() const // 붿붿试版本是뿯冽붿뿯皽
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
	return (CSmartGisDoc*)m_pDocument;
}
#endif //_DEBUG


// CSmartGisView 뿯涽息处理程뿯庽

void CSmartGisView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
}

int CSmartGisView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  在此뿯涽뿯劽뿯悽专用뿯皽创뿯庽뿯亽码
	return 0;
}

void CSmartGisView::OnDestroy()
{
	CView::OnDestroy();

	// TODO: 在此处뿯涽뿯劽뿯涽息处理程뿯庽뿯亽码
}

void CSmartGisView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	// TODO: 在此处뿯涽뿯劽뿯涽息处理程뿯庽뿯亽码
}

void CSmartGisView::OnLButtonDown(UINT nFlags, CPoint point)
{
	// Pass the message along
	CView::OnLButtonDown(nFlags, point);
} 


void CSmartGisView::OnMouseMove(UINT nFlags, CPoint point)
{
	// Pass the message along
	CView::OnMouseMove(nFlags, point);
} 

void CSmartGisView::OnLButtonUp (UINT nFlags, CPoint point)
{
	// Pass the message along
	CView::OnLButtonUp(nFlags, point);
} 


void CSmartGisView::OnRButtonDown(UINT nFlags, CPoint point)
{
	CView::OnRButtonDown(nFlags, point);
} 

BOOL CSmartGisView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// TODO: 在此뿯涽뿯劽뿯涽息处理程뿯庽뿯亽码뿯咽/或붿用默뿯붿值

	return CView::OnMouseWheel(nFlags, zDelta, pt);
}

void CSmartGisView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// TODO: 在此뿯涽뿯劽뿯涽息处理程뿯庽뿯亽码뿯咽/或붿用默뿯붿值
	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}
BOOL CSmartGisView::OnEraseBkgnd(CDC* pDC)
{
	// TODO: 在此뿯涽뿯劽뿯涽息处理程뿯庽뿯亽码뿯咽/或붿用默뿯붿值

	return TRUE;
}

void CSmartGisView::OnContextMenu(CWnd*pWnd, CPoint point)
{
	// TODO: 在此处뿯涽뿯劽뿯涽息处理程뿯庽뿯亽码
}

BOOL CSmartGisView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	return TRUE;
} 

void CSmartGisView::OnPrepareDC(CDC* pDC, CPrintInfo* pInfo)
{
	// TODO: 在此뿯涽뿯劽专用뿯亽码뿯咽/或붿用뿯垽类

	CView::OnPrepareDC(pDC, pInfo);
}