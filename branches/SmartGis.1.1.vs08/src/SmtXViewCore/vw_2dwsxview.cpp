// Smt2DWSXView.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtViewCore.h"
#include "vw_2dwsxview.h"

#include "smt_core.h"
#include "sys_sysmanager.h"
#include "bl_stylemanager.h"
#include "smt_logmanager.h"
#include "cata_mapmgr.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "smt_api.h"
#include "gt_defs.h"
#include "smt_listenermanager.h"
#include "am_amodulemanager.h"
#include "t_iatoolmanager.h"
#include "am_msg.h"

using namespace Smt_Core;
using namespace Smt_Geo;
using namespace Smt_GIS;
using namespace Smt_Rd;
using namespace Smt_GroupTool;
using namespace Smt_Sys;
using namespace Smt_AM;

// Smt2DWSXView

namespace Smt_XView
{
	IMPLEMENT_DYNCREATE(Smt2DWSXView, SmtXView)

		Smt2DWSXView::Smt2DWSXView()
	{
		m_pRenderer            = NULL;
		m_pRenderDevice        = NULL;

		m_pViewCtrlTool		  = NULL;
		m_pSelectTool		  = NULL;
		m_pFlashTool			  = NULL;

		m_pOperWSMap			  = NULL;
	}

	Smt2DWSXView::~Smt2DWSXView()
	{
	}

	BEGIN_MESSAGE_MAP(Smt2DWSXView, SmtXView)
		ON_WM_CREATE()
		ON_WM_DESTROY()
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
		ON_WM_SETCURSOR()
		ON_WM_ERASEBKGND()

	END_MESSAGE_MAP()


	// Smt2DWSXView 绘图

	void Smt2DWSXView::OnDraw(CDC* pDC)
	{
		CDocument* pDoc = GetDocument();
		// TODO: 在此添加绘制代码
		/*if (!m_bActive)
		return;*/

		if (m_pRenderDevice)
			m_pRenderDevice->RenderMap();

		if(m_pFlashTool)
			m_pFlashTool->AuxDraw();

		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->AuxDraw();
		}
	}

	// Smt2DWSXView 诊断

#ifdef _DEBUG
	void Smt2DWSXView::AssertValid() const
	{
		SmtXView::AssertValid();
	}

#ifndef _WIN32_WCE
	void Smt2DWSXView::Dump(CDumpContext& dc) const
	{
		SmtXView::Dump(dc);
	}
#endif
#endif //_DEBUG


	// Smt2DWSXView 消息处理程序

	int Smt2DWSXView::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (SmtXView::OnCreate(lpCreateStruct) == -1)
			return -1;

		// TODO:  在此添加您专用的创建代码
		SmtSysManager *pSysMgr = SmtSysManager::GetSingletonPtr();

		SmtSysPra sysPra = pSysMgr->GetSysPra();

		m_uiRefreshTimer = SetTimer( 69, sysPra.l2DViewRefreshTime, 0 );
		m_uiNotifyTimer = SetTimer( 70, sysPra.l2DViewNotifyTime, 0 );

		return 0;
	}

	void Smt2DWSXView::OnDestroy()
	{
		SmtXView::OnDestroy();

		// TODO: 在此处添加消息处理程序代码
		KillTimer(69);
		KillTimer(70);
	}

	void Smt2DWSXView::OnSize(UINT nType, int cx, int cy)
	{
		SmtXView::OnSize(nType, cx, cy);

		// TODO: 在此处添加消息处理程序代码
		if (m_pRenderDevice && cx > 0 && cy > 0)
		{
			SmtSysManager	*pSysMgr = SmtSysManager::GetSingletonPtr();
			SmtSysPra		sysPra = pSysMgr->GetSysPra();
			lRect			lrt;	
			Smt2DRenderPra	rdPra;

			lrt.lb.x = 0;
			lrt.rt.y = 0;
			lrt.rt.x = cx;
			lrt.lb.y = cy;

			rdPra.bShowMBR		= sysPra.bShowMBR;
			rdPra.bShowPoint	= sysPra.bShowPoint;
			rdPra.lPointRaduis	= sysPra.lPointRaduis;

			if (NULL != m_pOperWSMap)
			{
				lRect lrct;
				lrct.lb.x = 0;
				lrct.lb.y = 0;

				lrct.rt.x = cx;
				lrct.rt.y = cy;

				m_pOperWSMap->SetClientRect(lrct);
			}

			m_pRenderDevice->Resize(0,0,cx,cy);
			m_pRenderDevice->SetRenderPra(rdPra);
			m_pRenderDevice->RefreshDirectly(m_pOperWSMap,lrt);
		}
	}

	void Smt2DWSXView::OnMouseMove(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->MouseMove(nFlags,CPtTolPt(point));
		}

		SmtXView::OnMouseMove(nFlags, point);
	}

	void Smt2DWSXView::OnTimer(UINT_PTR nIDEvent)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		if (!m_bActive)
			return;

		switch(nIDEvent)
		{
		case 69:
			{
				SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
				SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
				if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
				{
					pTmpTool->Timer();
				}

				if(m_pFlashTool)
					m_pFlashTool->Timer();
			}
			break;
		case 70:
			{
				if(m_pRenderDevice && m_bActive)
				{
					SmtSysManager	*pSysMgr = SmtSysManager::GetSingletonPtr();
					SmtSysPra		sysPra = pSysMgr->GetSysPra();
					Smt2DRenderPra	rdPra;

					rdPra.bShowMBR		= sysPra.bShowMBR;
					rdPra.bShowPoint	= sysPra.bShowPoint;
					rdPra.lPointRaduis	= sysPra.lPointRaduis;

					m_pRenderDevice->SetRenderPra(rdPra);
					m_pRenderDevice->Timer();

					if (m_bActive)
						PostMessage( WM_PAINT);
				}
			}
			break;
		default:
			break;
		}

		SmtXView::OnTimer(nIDEvent);
	}

	void Smt2DWSXView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->KeyDown(nChar,nRepCnt,nFlags);
		}

		SmtXView::OnKeyDown(nChar, nRepCnt, nFlags);
	}

	void Smt2DWSXView::OnLButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->LButtonDown(nFlags,CPtTolPt(point));
		}

		SmtXView::OnLButtonDown(nFlags, point);
	}

	void Smt2DWSXView::OnLButtonUp(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->LButtonUp(nFlags,CPtTolPt(point));
		}

		SmtXView::OnLButtonUp(nFlags, point);
	}

	void Smt2DWSXView::OnRButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->RButtonDown(nFlags,CPtTolPt(point));
		}

		SmtXView::OnRButtonDown(nFlags, point);
	}

	BOOL Smt2DWSXView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->MouseWeel(nFlags,zDelta,CPtTolPt(pt));
		}

		return SmtXView::OnMouseWheel(nFlags, zDelta, pt);
	}

	void Smt2DWSXView::OnLButtonDblClk(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->LButtonDClick(nFlags,CPtTolPt(point));
		}

		SmtXView::OnLButtonDblClk(nFlags, point);
	}

	void Smt2DWSXView::OnRButtonDblClk(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->RButtonDClick(nFlags,CPtTolPt(point));
		}

		SmtXView::OnRButtonDblClk(nFlags, point);
	}

	void Smt2DWSXView::OnRButtonUp(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->RButtonUp(nFlags,CPtTolPt(point));
		}

		SmtXView::OnRButtonUp(nFlags, point);
	}

	BOOL Smt2DWSXView::OnEraseBkgnd(CDC* pDC)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		return FALSE;
	}

	void Smt2DWSXView::OnContextMenu(CWnd*pWnd, CPoint point)
	{
		// TODO: 在此处添加消息处理程序代码
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			if (pTmpTool && !pTmpTool->IsEnableContexMenu())
				return;
		}

		CMenu contexMenu;
		contexMenu.Attach(m_hContexMenu);
		contexMenu.TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,point.x,point.y,this);
		contexMenu.Detach();
	}

	BOOL Smt2DWSXView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
	{
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBaseTool *pTmpTool = dynamic_cast<SmtBaseTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			return pTmpTool->SetCursor();
		}

		return SmtXView::OnSetCursor(pWnd, nHitTest, message);
	} 

	//////////////////////////////////////////////////////////////////////////
	bool Smt2DWSXView::InitCreate(void)
	{
		return SmtXView::InitCreate();
	}

	bool Smt2DWSXView::EndDestory(void)
	{
		SmtGroupToolFactory::DestoryGroupTool(m_pViewCtrlTool);
		SmtGroupToolFactory::DestoryGroupTool(m_pSelectTool);
		SmtGroupToolFactory::DestoryGroupTool(m_pFlashTool);

		SMT_SAFE_DELETE(m_pRenderer);

		return SmtXView::EndDestory();
	}

	bool Smt2DWSXView::CreateContexMenu()
	{
		SmtXView::CreateContexMenu();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		//view ctrl menu
		AppendListenerMenu(m_hContexMenu,m_pViewCtrlTool,FIM_2DVIEW,false);

		//flash menu
		AppendListenerMenu(m_hContexMenu,m_pFlashTool,FIM_2DVIEW);

		//select menu
		AppendListenerMenu(m_hContexMenu,m_pSelectTool,FIM_2DVIEW);

		//////////////////////////////////////////////////////////////////////////
		//am menu
		/*::AppendMenu(m_hContexMenu,MF_SEPARATOR,NULL,NULL);
		SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();
		if (pAModuleMgr)
		{
		for (int i = 0; i < pAModuleMgr->GetAModuleCount();i ++)
		{
		SmtAuxModule *pAModule = pAModuleMgr->GetAModule(i);
		HMENU hMenu = CreateListenerMenu(pAModule,FIM_2DVIEW);
		if (GetMenuItemCount(hMenu) > 0)
		InsertMenu(m_hContexMenu,i+3,MF_POPUP,(UINT)hMenu,pAModule->GetName());
		}
		}*/

		pLog->LogMessage("Init 2DView ContexMenu OK!");

		return true;
	}

	bool Smt2DWSXView::CreateMainMenu()
	{
		SmtXView::CreateMainMenu();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		//view ctrl menu
		HMENU hMenu = CreateListenerMenu(m_pViewCtrlTool,FIM_2DMFMENU);
		if (GetMenuItemCount(hMenu) > 0)
			AppendMenu(m_hMainMenu,MF_POPUP,(UINT)hMenu,m_pViewCtrlTool->GetName());

		//flash menu
		hMenu = CreateListenerMenu(m_pFlashTool,FIM_2DMFMENU);
		if (GetMenuItemCount(hMenu) > 0)
			AppendMenu(m_hMainMenu,MF_POPUP,(UINT)hMenu,m_pFlashTool->GetName());

		//select menu
		hMenu = CreateListenerMenu(m_pSelectTool,FIM_2DMFMENU);
		if (GetMenuItemCount(hMenu) > 0)
			AppendMenu(m_hMainMenu,MF_POPUP,(UINT)hMenu,m_pSelectTool->GetName());

		////////////////////////////////////////////////////////////////////////////
		////am menu
		//::AppendMenu(m_hMainMenu,MF_SEPARATOR,NULL,NULL);
		//SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();
		//if (pAModuleMgr)
		//{
		//	for (int i = 0; i < pAModuleMgr->GetAModuleCount();i ++)
		//	{
		//		SmtAuxModule *pAModule = pAModuleMgr->GetAModule(i);
		//		HMENU hMenu = CreateListenerMenu(pAModule,FIM_2DMFMENU);
		//		if (GetMenuItemCount(hMenu) > 0)
		//			AppendMenu(m_hMainMenu,MF_POPUP,(UINT)hMenu,pAModule->GetName());
		//	}
		//}

		pLog->LogMessage("Init 2DView MainMenu OK!");

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool Smt2DWSXView::CreateRender(void)
	{
		SmtSysManager *pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();

		SmtLog *pLog = pLogMgr->GetDefaultLog();
		SmtSysPra sysPra = pSysMgr->GetSysPra();

		string strMapRenderDevice = sysPra.str2DRenderDeviceName;
		m_pRenderer = new SmtRenderer(AfxGetInstanceHandle());
		if (m_pRenderer->CreateDevice(strMapRenderDevice.c_str()) == SMT_ERR_FAILURE)
			return false;

		m_pRenderDevice = m_pRenderer->GetDevice();
		if (m_pRenderDevice == NULL)
			return false;

		if (m_pRenderDevice->Init(m_hWnd,strMapRenderDevice.c_str()) == SMT_ERR_FAILURE)
		{
			pLog->LogMessage("Init %s failure!",strMapRenderDevice.c_str());
			return false;
		}

		m_pRenderDevice->SetMapMode(MM_TEXT);

		pLog->LogMessage("Init %s ok!",strMapRenderDevice.c_str());

		return true;
	}

	bool Smt2DWSXView::CreateTools(void)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();

		SmtGroupToolFactory::CreateGroupTool(m_pViewCtrlTool,GroupToolType::GTT_WSViewControl);
		SmtGroupToolFactory::CreateGroupTool(m_pSelectTool,GroupToolType::GTT_Select);
		SmtGroupToolFactory::CreateGroupTool(m_pFlashTool,GroupToolType::GTT_Flash);

		if (NULL == m_pViewCtrlTool ||
			NULL == m_pSelectTool   ||
			NULL == m_pFlashTool )
		{
			pLog->LogMessage("Init GroupTools failure!");
			return false;
		}

		m_pViewCtrlTool->SetToolStyleName(pSysMgr->GetSysStyleConfig().szAuxStyle);
		if (m_pViewCtrlTool->Init(m_pRenderDevice,m_pOperWSMap,m_hWnd) != SMT_ERR_NONE)
			return false;

		m_pSelectTool->SetToolStyleName(pSysMgr->GetSysStyleConfig().szAuxStyle);
		if (m_pSelectTool->Init(m_pRenderDevice,m_pOperWSMap,m_hWnd) != SMT_ERR_NONE)
			return false;

		m_pFlashTool->SetToolStyleName(pSysMgr->GetSysStyleConfig().szAuxStyle);
		if (m_pFlashTool->Init(m_pRenderDevice,m_pOperWSMap,m_hWnd) != SMT_ERR_NONE)
			return false;

		m_pViewCtrlTool->SetActive();

		pLog->LogMessage("Init GroupTools ok!");

		return true;
	}

	void Smt2DWSXView::SetOperWSMap(SmtWSMap *pWSMap) 
	{
		m_pOperWSMap = pWSMap;

		if (m_pViewCtrlTool)
			m_pViewCtrlTool->SetOperMap(pWSMap);

		if (m_pSelectTool)
			m_pSelectTool->SetOperMap(pWSMap);

		if (m_pFlashTool)
			m_pFlashTool->SetOperMap(pWSMap);
	}

	BOOL Smt2DWSXView::OnCommand(WPARAM wParam, LPARAM lParam)
	{
		// TODO: 在此添加专用代码和/或调用基类
		unsigned int unMsg = (wParam);
		SmtListenerMsg param;
		param.hSrcWnd = m_hWnd;

		if (unMsg >= SMT_MSG_CMD_BEGIN && unMsg <= SMT_MSG_CMD_END)
		{
			SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
			pIAToolMgr->Notify(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(unMsg,m_hWnd),param);
		}
		else if (unMsg >= SMT_MSG_USER_BEGIN && unMsg <= SMT_MSG_USER_END)
		{
			SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();
			if (pAModuleMgr)
				pAModuleMgr->Notify(SMT_AM_MSG_BROADCAST,unMsg,param);
		}

		return SmtXView::OnCommand(wParam, lParam);
	}
}