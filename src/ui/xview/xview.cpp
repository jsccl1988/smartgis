// Smt2DXView.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "ui/xview/view_core.h"
#include "ui/xview/xview.h"
#include "ui/xview/view_chrome.h"
#include "content/public/view_host.h"
#include "base/core/logmanager.h"
#include "plugin/host/legacy_cmd.h"
#include "plugin/legacy/module_manager.h"
#include "legacy_tool/t_iatoolmanager.h"
#include "legacy_tool/t_msg.h"
#include "tool/workspace.h"
#include "base/core/msg.h"
#include "plugin/legacy/plugin_msg.h"

// SmtXView
using namespace base;

namespace ui
{
	IMPLEMENT_DYNCREATE(SmtXView, CView)

		SmtXView::SmtXView():m_bActive(TRUE)
		,m_pBindDlg(NULL)
		,m_unBindItemID(0)
		,m_pViewHost(NULL)
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
		// TODO: �ڴ����ӻ��ƴ���
	}

	int SmtXView::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (CView::OnCreate(lpCreateStruct) == -1)
			return -1;

		// TODO:  �ڴ�������ר�õĴ�������

		if (SMT_ERR_NONE != register_())
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

		// TODO: �ڴ˴�������Ϣ�����������
		EndDestory();

		unregister();
	}


	void SmtXView::OnInitialUpdate()
	{
		CView::OnInitialUpdate();

		// TODO: �ڴ�����ר�ô����/����û���
	}

	BOOL SmtXView::PreTranslateMessage(MSG* pMsg) 
	{
		if (NULL != m_pBindDlg)
			::ScreenToClient(m_hWnd,&pMsg->pt);

		return CView::PreTranslateMessage(pMsg);
	}

	LRESULT SmtXView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (dispatch_chrome_message(m_pViewHost, message, wParam, lParam))
		{
			if (m_pViewHost && m_pViewHost->workspace())
			{
				m_pViewHost->workspace()->aux_draw();
				if (m_pViewHost->workspace()->live_preview())
				{
					InvalidateRect(NULL, FALSE);
				}
			}
			if (message == WM_MOUSEWHEEL)
			{
				return TRUE;
			}
			return 0;
		}
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


	// SmtXView ��Ϣ��������
	void SmtXView::reset_view_host(content::ViewHost* host)
	{
		delete m_pViewHost;
		m_pViewHost = host;
		bind_draft_observer();
	}

	void SmtXView::bind_draft_observer()
	{
		if (!m_pViewHost || !m_pViewHost->workspace())
		{
			return;
		}
		m_pViewHost->workspace()->set_draft_observer(
			[this](const tool::Draft& d) { apply_workspace_draft(d); });
	}

	void SmtXView::apply_workspace_draft(const tool::Draft&) {}

	void SmtXView::dispatch_menu_command(unsigned int msg)
	{
		if (!route_chrome_command(msg))
		{
			return;
		}
		SmtListenerMsg param;
		param.hSrcWnd = m_hWnd;
		const long m = static_cast<long>(msg);
		if (msg >= SMT_MSG_CMD_BEGIN && msg <= SMT_MSG_CMD_END)
		{
			SmtIAToolManager* tools = SmtIAToolManager::get_singleton_ptr();
			if (tools)
			{
				tools->notify(SMT_IATOOL_MSG_BROADCAST,
					      SMT_MSG_KEY(m, m_hWnd), param);
			}
			return;
		}
		if (msg >= SMT_MSG_USER_BEGIN && msg <= SMT_MSG_USER_END)
		{
			SmtAModuleManager* modules = SmtAModuleManager::get_singleton_ptr();
			if (modules)
			{
				modules->notify(SMT_AM_MSG_BROADCAST, m, param);
			}
		}
	}

	bool SmtXView::route_chrome_command(unsigned int msg)
	{
		if (!m_pViewHost)
		{
			return false;
		}
		const long m = static_cast<long>(msg);
		if (m_pViewHost->execute_legacy(m))
		{
			return true;
		}
		const char* id = plugin::command_id_from_am_msg(m);
		if (id)
		{
			m_pViewHost->execute(id);
			return true;
		}
		m_pViewHost->release_exclusive();
		return false;
	}

	bool SmtXView::InitCreate(void) 
	{ 
		if (!m_pViewHost)
		{
			m_pViewHost = new content::ViewHost();
		}

		if (!CreateRender())
		{
			return false;
		}

		if (!CreateTools())
		{
			return false;
		}

		bind_draft_observer();

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
		reset_view_host(NULL);

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
		// TODO: �ڴ�����ר�ô����/����û���
		m_bActive = bActivate;

		CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
	}

	void SmtXView::PostNcDestroy()
	{
		// TODO: �ڴ�����ר�ô����/����û���
		if (NULL == m_pBindDlg)
			CView::PostNcDestroy();
	}

	void SmtXView::OnActivateFrame(UINT nState, CFrameWnd* pDeactivateFrame)
	{
		// TODO: �ڴ�����ר�ô����/����û���
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

	int SmtXView::notify(long nMsg,SmtListenerMsg &param)
	{
		return SMT_ERR_NONE;
	}
}