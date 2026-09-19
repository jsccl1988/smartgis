// child_frame.cpp : CChildFrame 锟斤拷锟绞碉拷锟?//
#include "legacy/app/stdafx.h"
#include "legacy/app/child_frame.h"

#include "base/core/log.h"
#include "legacy/app/smart_gis.h"

using namespace base;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CChildWnd)

BEGIN_MESSAGE_MAP(CChildFrame, CChildWnd)
ON_WM_CREATE()
ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

// CChildFrame 锟斤拷锟斤拷/锟斤拷锟斤拷

CChildFrame::CChildFrame() {
  // TODO: 锟节达拷锟斤拷锟接筹拷员锟斤拷始锟斤拷锟斤拷锟斤拷
}

CChildFrame::~CChildFrame() {}

BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs) {
  // TODO: 锟节此达拷通锟斤拷锟睫革拷 CREATESTRUCT cs
  // 锟斤拷锟睫改达拷锟斤拷锟斤拷锟斤拷锟绞?
  cs.style = (cs.style & (~WS_SYSMENU)) | WS_MAXIMIZEBOX;

  if (!CChildWnd::PreCreateWindow(cs)) return FALSE;

  return TRUE;
}

// CChildFrame 锟斤拷锟?
#ifdef _DEBUG
void CChildFrame::AssertValid() const { CChildWnd::AssertValid(); }

void CChildFrame::Dump(CDumpContext& dc) const { CChildWnd::Dump(dc); }

#endif  //_DEBUG

// CChildFrame 锟斤拷息锟斤拷锟斤拷锟斤拷锟斤拷

void CChildFrame::ActivateFrame(int nCmdShow) {
  // TODO: 锟节达拷锟斤拷锟斤拷专锟矫达拷锟斤拷锟?锟斤拷锟斤拷没锟斤拷锟?
  nCmdShow = SW_MAXIMIZE;
  CChildWnd::ActivateFrame(nCmdShow);
}

BOOL CChildFrame::OnCommand(WPARAM wParam, LPARAM lParam) {
  // TODO: 锟节达拷锟斤拷锟斤拷专锟矫达拷锟斤拷锟?锟斤拷锟斤拷没锟斤拷锟?
  CView* pView = GetActiveView();
  if (pView) {
    pView->PostMessage(WM_COMMAND, wParam, lParam);
  }

  return CBCGPMDIChildWnd::OnCommand(wParam, lParam);
}

int CChildFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CBCGPMDIChildWnd::OnCreate(lpCreateStruct) == -1) return -1;

  // TODO: add specialized creation code here
  return 0;
}

void CChildFrame::OnSysCommand(UINT nID, LPARAM lParam) {
  // TODO:
  // 锟节达拷锟斤拷锟斤拷锟斤拷息锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷/锟斤拷锟斤拷锟侥拷锟街?
  if (nID == WM_CLOSE) return;

  CBCGPMDIChildWnd::OnSysCommand(nID, lParam);
}

BOOL CChildFrame::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName,
                         DWORD dwStyle, const RECT& rect,
                         CMDIFrameWnd* pParentWnd, CCreateContext* pContext) {
  // TODO: 锟节达拷锟斤拷锟斤拷专锟矫达拷锟斤拷锟?锟斤拷锟斤拷没锟斤拷锟?
  dwStyle = (dwStyle & (~WS_SYSMENU)) | WS_MAXIMIZEBOX;
  return CBCGPMDIChildWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect,
                                  pParentWnd, pContext);
}
