// Smt2DEditXView.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtViewCore.h"
#include "vw_2deditxview.h"
#include "smt_core.h"
#include "sys_sysmanager.h"
#include "bl_stylemanager.h"
#include "smt_logmanager.h"
#include "cata_mapmgr.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "smt_api.h"
#include "gt_defs.h"

using namespace Smt_Core;
using namespace Smt_Geo;
using namespace Smt_GIS;
using namespace Smt_Rd;
using namespace Smt_GroupTool;
using namespace Smt_Sys;

// Smt2DEditXView

namespace Smt_XView
{
	IMPLEMENT_DYNCREATE(Smt2DEditXView, Smt2DXView)

	Smt2DEditXView::Smt2DEditXView()
	{
		m_pAppendFeaTool		 = NULL;
	}

	Smt2DEditXView::~Smt2DEditXView()
	{
		;
	}

	BEGIN_MESSAGE_MAP(Smt2DEditXView, CView)
		ON_WM_SIZE()
		ON_WM_MOUSEMOVE()
		ON_WM_TIMER()
		ON_WM_KEYDOWN()
		ON_WM_LBUTTONDOWN()
		ON_WM_LBUTTONUP()
		ON_WM_RBUTTONDOWN()
		ON_WM_MOUSEWHEEL()
		ON_WM_LBUTTONDBLCLK()
		ON_WM_RBUTTONDBLCLK()
		ON_WM_RBUTTONUP()
		ON_WM_CONTEXTMENU()
		ON_WM_ERASEBKGND()
		ON_WM_SETCURSOR()

	END_MESSAGE_MAP()


	// Smt2DEditXView 绘图

	void Smt2DEditXView::OnDraw(CDC* pDC)
	{
		CDocument* pDoc = GetDocument();
		// TODO: 在此添加绘制代码
		Smt2DXView::OnDraw(pDC);
	}


	void Smt2DEditXView::OnSize(UINT nType, int cx, int cy)
	{
		Smt2DXView::OnSize(nType, cx, cy);

		// TODO: 在此处添加消息处理程序代码
	}

	// Smt2DEditXView 诊断

#ifdef _DEBUG
	void Smt2DEditXView::AssertValid() const
	{
		CView::AssertValid();
	}

#ifndef _WIN32_WCE
	void Smt2DEditXView::Dump(CDumpContext& dc) const
	{
		CView::Dump(dc);
	}
#endif
#endif //_DEBUG


	// Smt2DEditXView 消息处理程序
	//////////////////////////////////////////////////////////////////////////
	void Smt2DEditXView::OnMouseMove(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		Smt2DXView::OnMouseMove(nFlags, point);
	}

	void Smt2DEditXView::OnTimer(UINT_PTR nIDEvent)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		Smt2DXView::OnTimer(nIDEvent);
	}

	void Smt2DEditXView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		Smt2DXView::OnKeyDown(nChar, nRepCnt, nFlags);
	}

	void Smt2DEditXView::OnLButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		Smt2DXView::OnLButtonDown(nFlags, point);
	}

	void Smt2DEditXView::OnLButtonUp(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		Smt2DXView::OnLButtonUp(nFlags, point);
	}

	void Smt2DEditXView::OnRButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		Smt2DXView::OnRButtonDown(nFlags, point);
	}

	BOOL Smt2DEditXView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		return Smt2DXView::OnMouseWheel(nFlags, zDelta, pt);
	}

	void Smt2DEditXView::OnLButtonDblClk(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		Smt2DXView::OnLButtonDblClk(nFlags, point);
	}

	void Smt2DEditXView::OnRButtonDblClk(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		Smt2DXView::OnRButtonDblClk(nFlags, point);
	}

	void Smt2DEditXView::OnRButtonUp(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		Smt2DXView::OnRButtonUp(nFlags, point);
	}

	BOOL Smt2DEditXView::OnEraseBkgnd(CDC* pDC)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		return Smt2DXView::OnEraseBkgnd(pDC);
	}

	void Smt2DEditXView::OnContextMenu(CWnd*pWnd, CPoint point)
	{
		// TODO: 在此处添加消息处理程序代码
		Smt2DXView::OnContextMenu(pWnd,point);
	}

	BOOL Smt2DEditXView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
	{
		return Smt2DXView::OnSetCursor(pWnd,nHitTest,message);
	} 

	//////////////////////////////////////////////////////////////////////////
	bool Smt2DEditXView::InitCreate(void)
	{
		return Smt2DXView::InitCreate();
	}

	bool Smt2DEditXView::EndDestory(void)
	{
		SmtGroupToolFactory::DestoryGroupTool(m_pAppendFeaTool);

		return Smt2DXView::EndDestory();
	}

	bool Smt2DEditXView::CreateTools(void)
	{
		Smt2DXView::CreateTools();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		//sys style
		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

		//创建tool
		SmtGroupToolFactory::CreateGroupTool(m_pAppendFeaTool,GroupToolType::GTT_AppendFeature);

		if (NULL == m_pAppendFeaTool)
		{
			return false;
		}

		//设置tool
		m_pAppendFeaTool->SetToolStyleName(styleSonfig.szAuxStyle);

		if (m_pAppendFeaTool->Init(m_pRenderDevice,m_pSmtOperMap,m_hWnd) != SMT_ERR_NONE)
			return false;

		pLog->LogMessage("Init 2DEditView GroupTools OK!");

		return true;
	}

	void Smt2DEditXView::SetOperMap(SmtMap *pSmtMap)
	{
		Smt2DXView::SetOperMap(pSmtMap);

		if (m_pAppendFeaTool)
			m_pAppendFeaTool->SetOperMap(pSmtMap);
	}

	bool Smt2DEditXView::CreateContexMenu()
	{
		Smt2DXView::CreateContexMenu();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		//append feature menu
		AppendListenerMenu(m_hContexMenu,m_pAppendFeaTool,FIM_2DVIEW);

		pLog->LogMessage("Init 2DEditView ContexMenu OK!");

		return true;
	}

	bool Smt2DEditXView::CreateMainMenu()
	{
		Smt2DXView::CreateMainMenu();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		HMENU hMenu = CreateListenerMenu(m_pAppendFeaTool,FIM_2DMFMENU);
		if (GetMenuItemCount(hMenu) > 0)
			::InsertMenu(m_hMainMenu,0,MF_POPUP|MF_BYPOSITION,(UINT)hMenu,m_pAppendFeaTool->GetName());

		pLog->LogMessage("Init 2DEditView MainMenu OK!");

		return true;
	}
}