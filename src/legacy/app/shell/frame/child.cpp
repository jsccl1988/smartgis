#include "legacy/app/stdafx.h"

#include "legacy/app/shell/frame/child.h"

#include "base/core/log.h"
#include "legacy/app/shell/frame/app.h"

using namespace base;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


IMPLEMENT_DYNCREATE(CChildFrame, CChildWnd)

BEGIN_MESSAGE_MAP(CChildFrame, CChildWnd)
ON_WM_CREATE()
ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()


CChildFrame::CChildFrame() {
}

CChildFrame::~CChildFrame() {}

BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs) {
  cs.style = (cs.style & (~WS_SYSMENU)) | WS_MAXIMIZEBOX;

  if (!CChildWnd::PreCreateWindow(cs)) return FALSE;

  return TRUE;
}

#ifdef _DEBUG
void CChildFrame::AssertValid() const { CChildWnd::AssertValid(); }

void CChildFrame::Dump(CDumpContext& dc) const { CChildWnd::Dump(dc); }

#endif  //_DEBUG


void CChildFrame::ActivateFrame(int nCmdShow) {
  nCmdShow = SW_MAXIMIZE;
  CChildWnd::ActivateFrame(nCmdShow);
}

BOOL CChildFrame::OnCommand(WPARAM wParam, LPARAM lParam) {
  CView* pView = GetActiveView();
  if (pView) {
    pView->PostMessage(WM_COMMAND, wParam, lParam);
  }

  return CBCGPMDIChildWnd::OnCommand(wParam, lParam);
}

int CChildFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CBCGPMDIChildWnd::OnCreate(lpCreateStruct) == -1) return -1;

  return 0;
}

void CChildFrame::OnSysCommand(UINT nID, LPARAM lParam) {
  if (nID == WM_CLOSE) return;

  CBCGPMDIChildWnd::OnSysCommand(nID, lParam);
}

BOOL CChildFrame::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName,
                         DWORD dwStyle, const RECT& rect,
                         CMDIFrameWnd* pParentWnd, CCreateContext* pContext) {
  dwStyle = (dwStyle & (~WS_SYSMENU)) | WS_MAXIMIZEBOX;
  return CBCGPMDIChildWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect,
                                  pParentWnd, pContext);
}
