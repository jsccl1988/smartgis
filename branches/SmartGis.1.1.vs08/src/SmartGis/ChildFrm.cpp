// ChildFrm.cpp : CChildFrame 类的实现
//
#include "stdafx.h"
#include "SmartGis.h"

#include "ChildFrm.h"

#include "smt_logmanager.h"

using namespace Smt_Core;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CChildWnd)

BEGIN_MESSAGE_MAP(CChildFrame, CChildWnd)
	ON_WM_CREATE()
	ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()


// CChildFrame 构造/析构

CChildFrame::CChildFrame()
{
	// TODO: 在此添加成员初始化代码
}

CChildFrame::~CChildFrame()
{
}


BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: 在此处通过修改 CREATESTRUCT cs 来修改窗口类或样式
	cs.style = (cs.style & (~WS_SYSMENU))|WS_MAXIMIZEBOX;

	if( !CChildWnd::PreCreateWindow(cs) )
		return FALSE;

	return TRUE;
}


// CChildFrame 诊断

#ifdef _DEBUG
void CChildFrame::AssertValid() const
{
	CChildWnd::AssertValid();
}

void CChildFrame::Dump(CDumpContext& dc) const
{
	CChildWnd::Dump(dc);
}

#endif //_DEBUG


// CChildFrame 消息处理程序

void CChildFrame::ActivateFrame(int nCmdShow)
{
	// TODO: 在此添加专用代码和/或调用基类
	nCmdShow = SW_MAXIMIZE;
	CChildWnd::ActivateFrame(nCmdShow);
}

BOOL CChildFrame::OnCommand(WPARAM wParam, LPARAM lParam)
{
	// TODO: 在此添加专用代码和/或调用基类
	CView *pView = GetActiveView();
	if (pView)
	{
		pView->PostMessage(WM_COMMAND,wParam,lParam);
	}

	return CBCGPMDIChildWnd::OnCommand(wParam, lParam);
}

int CChildFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CBCGPMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  在此添加您专用的创建代码
	return 0;
}

void CChildFrame::OnSysCommand(UINT nID, LPARAM lParam)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	if (nID == WM_CLOSE)
		return;
	 
	CBCGPMDIChildWnd::OnSysCommand(nID, lParam);
}

BOOL CChildFrame::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle , const RECT& rect , CMDIFrameWnd* pParentWnd , CCreateContext* pContext)
{
	// TODO: 在此添加专用代码和/或调用基类
	dwStyle = (dwStyle & (~WS_SYSMENU))|WS_MAXIMIZEBOX;
	return CBCGPMDIChildWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, pContext);
}
