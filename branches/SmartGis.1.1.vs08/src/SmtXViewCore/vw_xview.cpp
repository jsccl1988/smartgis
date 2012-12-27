// Smt2DXView.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "SmtViewCore.h"
#include "vw_xview.h"
#include "smt_logmanager.h"

// SmtXView
using namespace Smt_Core;

namespace Smt_XView
{
	IMPLEMENT_DYNCREATE(SmtXView, CView)

		SmtXView::SmtXView():m_bActive(TRUE)
		,m_pBindDlg(NULL)
		,m_unBindItemID(0)
	{
		m_hContexMenu = NULL;
		m_hMainMenu = NULL;
	}

	SmtXView::~SmtXView()
	{

	}

	BEGIN_MESSAGE_MAP(SmtXView, CView)
		ON_WM_CREATE()
		ON_WM_DESTROY()
	END_MESSAGE_MAP()


	// Smt2DXView ��ͼ

	void SmtXView::OnDraw(CDC* pDC)
	{
		CDocument* pDoc = GetDocument();
		// TODO: �ڴ���ӻ��ƴ���
	}

	int SmtXView::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (CView::OnCreate(lpCreateStruct) == -1)
			return -1;

		// TODO:  �ڴ������ר�õĴ�������

		if (SMT_ERR_NONE != Register())
		{
			return -1;
		}

		if (!InitCreate())
		{
			return -1;
		}

		return 0;
	}

	void SmtXView::OnDestroy()
	{
		if (NULL != m_pBindDlg)
			CWnd::OnDestroy();
		else 
			CView::OnDestroy();

		// TODO: �ڴ˴������Ϣ����������
		EndDestory();

		UnRegister();
	}


	void SmtXView::OnInitialUpdate()
	{
		CView::OnInitialUpdate();

		// TODO: �ڴ����ר�ô����/����û���
	}

	BOOL SmtXView::PreTranslateMessage(MSG* pMsg) 
	{
		if (NULL != m_pBindDlg)
			::ScreenToClient(m_hWnd,&pMsg->pt);

		return CView::PreTranslateMessage(pMsg);
	}

	LRESULT SmtXView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		return CView::WindowProc(message, wParam, lParam);
	}


	// Smt2DXView ���

#ifdef _DEBUG
	void SmtXView::AssertValid() const
	{
		CView::AssertValid();
	}

#ifndef _WIN32_WCE
	void SmtXView::Dump(CDumpContext& dc) const
	{
		CView::Dump(dc);
	}
#endif
#endif //_DEBUG


	// SmtXView ��Ϣ�������
	bool SmtXView::InitCreate(void) 
	{ 
		if (!CreateRender())
		{
			return false;
		}

		if (!CreateTools())
		{
			return false;
		}

		if (!CreateMainMenu())
		{
			return false;
		}

		if (!CreateContexMenu())
		{
			return false;
		}

		return true;
	}

	bool SmtXView::EndDestory(void) 
	{ 
		::DestroyMenu(m_hMainMenu);
		::DestroyMenu(m_hContexMenu);

		return true;
	}

	bool SmtXView::CreateMainMenu(void) 
	{ 
		m_hMainMenu = ::CreatePopupMenu();
		return true;
	}

	bool SmtXView::CreateContexMenu(void) 
	{ 
		m_hContexMenu = ::CreatePopupMenu();
		return true;
	}

	void SmtXView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView)
	{
		// TODO: �ڴ����ר�ô����/����û���
		m_bActive = bActivate;

		CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
	}

	void SmtXView::PostNcDestroy()
	{
		// TODO: �ڴ����ר�ô����/����û���
		if (NULL == m_pBindDlg)
			CView::PostNcDestroy();
	}

	void SmtXView::OnActivateFrame(UINT nState, CFrameWnd* pDeactivateFrame)
	{
		// TODO: �ڴ����ר�ô����/����û���
		if (NULL == m_pBindDlg)
			CView::OnActivateFrame(nState, pDeactivateFrame);
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtXView::BindWind(HWND hWnd)
	{
		if (!CWnd::Attach(hWnd))
			return SMT_ERR_FAILURE;

		if (!InitCreate())
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	long SmtXView::BindDlgItem(CDialog *pDlg,UINT nItemID)
	{
		if (NULL == pDlg &&
			NULL == pDlg->GetSafeHwnd())
			return SMT_ERR_INVALID_PARAM;

		CWnd *pItemWnd = pDlg->GetDlgItem(nItemID);

		if (NULL == pItemWnd)
			return SMT_ERR_FAILURE;

		CRect rcItem;
		pItemWnd->GetWindowRect(rcItem);
		pDlg->ScreenToClient(rcItem);

		UINT style = ::GetWindowLong(pItemWnd->GetSafeHwnd(), GWL_STYLE);

		if(!pItemWnd->DestroyWindow())
			return SMT_ERR_FAILURE;

		ModifyStyle(0,SS_NOTIFY,TRUE);

		if(!Create(NULL, NULL, style | WS_CHILD | WS_VISIBLE , rcItem, pDlg, nItemID, NULL))
			return SMT_ERR_FAILURE;

		m_unBindItemID = nItemID;
		m_pBindDlg = pDlg;

		return SMT_ERR_NONE;
	}

	long SmtXView::UnbindWind(void)
	{
		if (NULL != m_pBindDlg)
		{
			CWnd *pItemWnd = m_pBindDlg->GetDlgItem(m_unBindItemID);
			pItemWnd->DestroyWindow();

			m_unBindItemID = 0;
			m_pBindDlg = NULL;
		}

		return SMT_ERR_NONE;
	}

	int SmtXView::Notify(long nMsg,SmtListenerMsg &param)
	{
		return SMT_ERR_NONE;
	}
}