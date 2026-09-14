// child_frame.cpp : CChildFrame ���ʵ��
//
#include "legacy_app/stdafx.h"
#include "legacy_app/smart_gis.h"

#include "legacy_app/child_frame.h"

#include "base/core/logmanager.h"

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


// CChildFrame ����/����

CChildFrame::CChildFrame()
{
	// TODO: �ڴ����ӳ�Ա��ʼ������
}

CChildFrame::~CChildFrame()
{
}


BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: �ڴ˴�ͨ���޸� CREATESTRUCT cs ���޸Ĵ��������ʽ
	cs.style = (cs.style & (~WS_SYSMENU))|WS_MAXIMIZEBOX;

	if( !CChildWnd::PreCreateWindow(cs) )
		return FALSE;

	return TRUE;
}


// CChildFrame ���

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


// CChildFrame ��Ϣ��������

void CChildFrame::ActivateFrame(int nCmdShow)
{
	// TODO: �ڴ�����ר�ô����/����û���
	nCmdShow = SW_MAXIMIZE;
	CChildWnd::ActivateFrame(nCmdShow);
}

BOOL CChildFrame::OnCommand(WPARAM wParam, LPARAM lParam)
{
	// TODO: �ڴ�����ר�ô����/����û���
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

	// TODO:  �ڴ�������ר�õĴ�������
	return 0;
}

void CChildFrame::OnSysCommand(UINT nID, LPARAM lParam)
{
	// TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ
	if (nID == WM_CLOSE)
		return;
	 
	CBCGPMDIChildWnd::OnSysCommand(nID, lParam);
}

BOOL CChildFrame::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle , const RECT& rect , CMDIFrameWnd* pParentWnd , CCreateContext* pContext)
{
	// TODO: �ڴ�����ר�ô����/����û���
	dwStyle = (dwStyle & (~WS_SYSMENU))|WS_MAXIMIZEBOX;
	return CBCGPMDIChildWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, pContext);
}
