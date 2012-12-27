// Smt3DXView.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtViewCore.h"
#include "vw_3dxview.h"
#include "smt_api.h"
#include "smt_logmanager.h"
#include "smt_listenermanager.h"
#include "am_amodulemanager.h"
#include "t_iatoolmanager.h"
#include "sys_sysmanager.h"
#include "am_msg.h"

using namespace Smt_3DBase;
using namespace Smt_Sys;
using namespace Smt_AM;

// Smt3DXView

namespace Smt_XView
{
	IMPLEMENT_DYNCREATE(Smt3DXView, SmtXView)

		Smt3DXView::Smt3DXView():m_p3DRenderer(NULL)
		,m_p3DRenderDevice(NULL)
		,m_pScene(NULL)
		,m_p3DViewCtrlTool(NULL)
	{
		;
	}

	Smt3DXView::~Smt3DXView()
	{
		SMT_SAFE_DELETE(m_pScene);
	}

	BEGIN_MESSAGE_MAP(Smt3DXView, SmtXView)
		ON_WM_SIZE()
		ON_WM_CREATE()
		ON_WM_DESTROY()
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


	// Smt3DXView 绘图

	void Smt3DXView::OnDraw(CDC* pDC)
	{
		CDocument* pDoc = GetDocument();
		// TODO: 在此添加绘制代码
		/*if (!m_bActive)
		return;*/

		if ( m_p3DRenderDevice == NULL )
		{
			pDC->SetBkColor( 0xFFFF0000 );
		}
		else
		{
			SmtSysManager *pSysMgr = SmtSysManager::GetSingletonPtr();
			SmtSysPra sysPra = pSysMgr->GetSysPra();

			m_p3DRenderDevice->SetClearColor(SmtColor(GetRValue(sysPra.l3DViewClearColor)/255.,GetGValue(sysPra.l3DViewClearColor)/255.,GetBValue(sysPra.l3DViewClearColor)/255.,1));
			m_p3DRenderDevice->Clear( CLR_COLOR | CLR_ZBUFFER );
			m_p3DRenderDevice->BeginRender();

			if (m_pScene)
				m_pScene->Render();

			SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
			SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
			if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
			{
				pTmpTool->AuxDraw();
			}

			m_p3DRenderDevice->EndRender();
			m_p3DRenderDevice->SwapBuffers();
		}
	}

	// Smt3DXView 诊断

#ifdef _DEBUG
	void Smt3DXView::AssertValid() const
	{
		SmtXView::AssertValid();
	}

#ifndef _WIN32_WCE
	void Smt3DXView::Dump(CDumpContext& dc) const
	{
		SmtXView::Dump(dc);
	}
#endif
#endif //_DEBUG


	// Smt3DXView 消息处理程序

	void Smt3DXView::OnSize(UINT nType, int cx, int cy)
	{
		SmtXView::OnSize(nType, cx, cy);

		// TODO: 在此处添加消息处理程序代码
		if (m_p3DRenderDevice)
		{
			Viewport3D viewport; 
			CRect rect;
			GetClientRect(&rect);

			viewport.ulHeight = rect.Height();
			viewport.ulWidth  = rect.Width();
			viewport.ulX = 0;
			viewport.ulY = 0;
			viewport.fZNear = 0.1;
			viewport.fZFar = 1000;
			viewport.fFovy = 45.f;

			// set viewport
			m_p3DRenderDevice->SetViewport(viewport);

			m_p3DRenderDevice->MatrixModeSet(MM_PROJECTION);
			m_p3DRenderDevice->MatrixLoadIdentity();
			m_p3DRenderDevice->SetPerspective(viewport.fFovy,(float)viewport.ulWidth/(float)viewport.ulHeight,viewport.fZNear,viewport.fZFar);
			m_p3DRenderDevice->MatrixModeSet(MM_MODELVIEW);
			m_p3DRenderDevice->MatrixLoadIdentity();

			if (m_p3DViewCtrlTool)
			{
				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				m_p3DViewCtrlTool->Notify(GT_MSG_3DVIEW_RESIZE,param);
			}
		}
	}

	int Smt3DXView::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (SmtXView::OnCreate(lpCreateStruct) == -1)
			return -1;

		// TODO:  在此添加您专用的创建代码
		SmtSysManager *pSysMgr = SmtSysManager::GetSingletonPtr();

		SmtSysPra sysPra = pSysMgr->GetSysPra();

		m_uiRefreshTimer = SetTimer( 69, sysPra.l3DViewRefreshTime, 0 );

		return 0;
	}

	void Smt3DXView::OnDestroy()
	{
		SmtXView::OnDestroy();

		// TODO: 在此处添加消息处理程序代码
		KillTimer( 69 );
	}

	void Smt3DXView::OnMouseMove(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->MouseMove(nFlags,CPtTolPt(point));
		}

		Vector3 vOrg;

		if (m_pScene)
			m_pScene->Transform2DTo3D(vOrg,m_vCursor3DPos,lPoint(point.x,point.y));

		SmtXView::OnMouseMove(nFlags, point);
	}

	void Smt3DXView::OnTimer(UINT_PTR nIDEvent)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		if ( nIDEvent==69) // Refresh timer
		{
			if (m_pScene)
				m_pScene->Update();

			SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
			SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
			if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
			{
				pTmpTool->Timer();
			}

			if (m_bActive)
				PostMessage( WM_PAINT);
		}

		SmtXView::OnTimer(nIDEvent);
	}

	void Smt3DXView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->KeyDown(nChar,nRepCnt,nFlags);
		}

		SmtXView::OnKeyDown(nChar, nRepCnt, nFlags);
	}

	void Smt3DXView::OnLButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->LButtonDown(nFlags,CPtTolPt(point));
		}

		SmtXView::OnLButtonDown(nFlags, point);
	}

	void Smt3DXView::OnLButtonUp(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->LButtonUp(nFlags,CPtTolPt(point));
		}

		SmtXView::OnLButtonUp(nFlags, point);
	}

	void Smt3DXView::OnRButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->RButtonDown(nFlags,CPtTolPt(point));
		}

		SmtXView::OnRButtonDown(nFlags, point);
	}

	BOOL Smt3DXView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->MouseWeel(nFlags,zDelta,CPtTolPt(pt));
		}

		return SmtXView::OnMouseWheel(nFlags, zDelta, pt);
	}

	void Smt3DXView::OnLButtonDblClk(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->LButtonDClick(nFlags,CPtTolPt(point));
		}

		SmtXView::OnLButtonDblClk(nFlags, point);
	}

	void Smt3DXView::OnRButtonDblClk(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->RButtonDClick(nFlags,CPtTolPt(point));
		}

		SmtXView::OnRButtonDblClk(nFlags, point);
	}

	void Smt3DXView::OnRButtonUp(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			pTmpTool->RButtonUp(nFlags,CPtTolPt(point));
		}

		SmtXView::OnRButtonUp(nFlags, point);
	}

	BOOL Smt3DXView::OnEraseBkgnd(CDC* pDC)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		return FALSE;
	}

	void Smt3DXView::OnContextMenu(CWnd*pWnd, CPoint point)
	{
		// TODO: 在此处添加消息处理程序代码
		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			if (!pTmpTool->IsEnableContexMenu())
				return;
		}

		::TrackPopupMenu(m_hContexMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,point.x,point.y,0,this->m_hWnd,NULL);
	}

	BOOL Smt3DXView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		if (nHitTest != HTCLIENT)
			return SmtXView::OnSetCursor(pWnd, nHitTest, message);

		SmtIAToolManager *pIAToolMgr = SmtIAToolManager::GetSingletonPtr();
		SmtBase3DTool *pTmpTool = dynamic_cast<SmtBase3DTool*>(pIAToolMgr->GetActiveIATool());
		if (pTmpTool && (pTmpTool->GetOwnerWnd() == m_hWnd))
		{
			return pTmpTool->SetCursor();
		}

		return TRUE;
	}

	//////////////////////////////////////////////////////////////////////////
	bool Smt3DXView::InitCreate(void)
	{
		return SmtXView::InitCreate();
	}

	bool Smt3DXView::CreateContexMenu()
	{
		SmtXView::CreateContexMenu();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		//3dview ctrl menu
		AppendListenerMenu(m_hContexMenu,m_p3DViewCtrlTool,FIM_3DVIEW,false);

		//////////////////////////////////////////////////////////////////////////
		//am menu
		::AppendMenu(m_hContexMenu,MF_SEPARATOR,NULL,NULL);
		SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();
		if (pAModuleMgr)
		{
			for (int i = 0; i < pAModuleMgr->GetAModuleCount();i ++)
			{
				SmtAuxModule *pAModule = pAModuleMgr->GetAModule(i);
				HMENU hMenu = CreateListenerMenu(pAModule,FIM_3DVIEW);
				if (GetMenuItemCount(hMenu) > 0)
					InsertMenu(m_hContexMenu,i+3,MF_POPUP,(UINT)hMenu,pAModule->GetName());
			}
		}

		pLog->LogMessage("Init 3DView ContexMenu ok!");

		return true;
	}

	bool Smt3DXView::CreateMainMenu()
	{
		SmtXView::CreateMainMenu();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		//view ctrl menu
		HMENU hMenu = CreateListenerMenu(m_p3DViewCtrlTool,FIM_3DMFMENU);
		if (GetMenuItemCount(hMenu) > 0)
			AppendMenu(m_hMainMenu,MF_POPUP,(UINT)hMenu,m_p3DViewCtrlTool->GetName());

		//////////////////////////////////////////////////////////////////////////
		//am menu
		::AppendMenu(m_hMainMenu,MF_SEPARATOR,NULL,NULL);
		SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();
		if (pAModuleMgr)
		{
			for (int i = 0; i < pAModuleMgr->GetAModuleCount();i ++)
			{
				SmtAuxModule *pAModule = pAModuleMgr->GetAModule(i);
				HMENU hMenu = CreateListenerMenu(pAModule,FIM_3DMFMENU);
				if (GetMenuItemCount(hMenu) > 0)
					AppendMenu(m_hMainMenu,MF_POPUP,(UINT)hMenu,pAModule->GetName());
			}
		}

		pLog->LogMessage("Init 2DView MainMenu OK!");

		return true;
	}

	bool Smt3DXView::EndDestory(void)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		SmtGroupToolFactory::DestoryGroup3DTool(m_p3DViewCtrlTool);

		pLog->LogMessage("Destory Group3DTools ok!");

		if (m_p3DRenderDevice)
		{
			m_p3DRenderDevice->Destroy();
			m_p3DRenderDevice = NULL;
		}

		pLog->LogMessage("Destory 3D RenderDevice ok!");

		SMT_SAFE_DELETE(m_p3DRenderer);

		return SmtXView::EndDestory();
	}

	//////////////////////////////////////////////////////////////////////////
	bool Smt3DXView::CreateRender(void)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();

		SmtLog *pLog = pLogMgr->GetDefaultLog();
		SmtSysPra sysPra = pSysMgr->GetSysPra();
		string str3DRenderDevice = sysPra.str3DRenderDeviceName;

		m_p3DRenderer = new Smt3DRenderer(AfxGetInstanceHandle());

		if(SMT_OK != m_p3DRenderer->CreateDevice(str3DRenderDevice.c_str()))
			return false;		

		m_p3DRenderDevice = m_p3DRenderer->GetDevice();
		if (m_p3DRenderDevice == NULL)
			return false;

		if(SMT_OK != m_p3DRenderDevice->Init(m_hWnd,"SmtGL3DRenderDevice"))
			return false;

		SMT_SAFE_DELETE(m_pScene);
		m_pScene = new SmtScene();

		//启动场景
		m_pScene->Set3DRenderDevice(m_p3DRenderDevice);

		if(m_pScene->Setup() != SMT_ERR_NONE)
			return false;

		pLog->LogMessage("Init 3D RenderDevice ok!");

		return Setup();
	}

	bool Smt3DXView::Setup(void)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		SmtLight lgt;

		lgt.SetType(LGT_DIRECTIONAL);
		lgt.SetAmbientValue(SmtColor(0.,0.,0.,1.0));
		lgt.SetDiffuseValue(SmtColor(1.0,1.0,1.0,1.0));
		lgt.SetSpecularValue(SmtColor(0.,0.,0.,1));
		lgt.SetPoistion(Vector4(1.0,1.0,1.0,0.0));
		lgt.SetDirection(Vector4(0.0,-1.0,-1.0,0));

		//clear clr
		//m_p3DRenderDevice->SetClearColor(SmtColor(0.9,0.9,0.9));
		m_p3DRenderDevice->SetClearColor(SmtColor(0.,0.,0.));
		m_p3DRenderDevice->SetDepthClearValue( 1.0f );
		m_p3DRenderDevice->SetStencilClearValue( 0 );

		// light stuff
		m_p3DRenderDevice->SetLight(0,&lgt);

		lgt.SetPoistion(Vector4(1.0,1.0,1.0,0.0));
		lgt.SetDirection(Vector4(-1.0,-1.0,-1.0,0));

		m_p3DRenderDevice->SetLight(1,&lgt);

		// fog stuff
		m_p3DRenderDevice->SetFog(FM_NONE,SmtColor(0.8,0.8,0.8),0.1,30,100);

		m_p3DRenderDevice->SetBackfaceCulling(RSV_CULL_NONE);
		m_p3DRenderDevice->SetDepthBufferMode(RSV_DEPTH_READWRITE);
		m_p3DRenderDevice->SetShadeMode(RSV_SHADE_SOLID,0,SmtColor(1.,1.,1.,1.0));
		m_p3DRenderDevice->SetAmbientLight(SmtColor(1.,1.,1.,1.0));

		string			strAppPath = GetAppPath();
		string			strTextureDir = strAppPath + "rs\\texture\\";
		string			strTexturePath = "";
		SmtTexture		*pTexture = NULL;

		//
		strTexturePath = strTextureDir + "terrain-1.bmp";
		pTexture = m_p3DRenderDevice->CreateTexture("terrain");
		pTexture->Load(strTexturePath.c_str());

		//
		strTexturePath = strTextureDir + "earth-1.tga";
		pTexture = m_p3DRenderDevice->CreateTexture("earth");
		pTexture->Load(strTexturePath.c_str());

		//
		strTexturePath = strTextureDir + "water.bmp";
		pTexture = m_p3DRenderDevice->CreateTexture("seawater");
		pTexture->Load(strTexturePath.c_str());

		//
		strTexturePath = strTextureDir + "rbed.bmp";
		pTexture = m_p3DRenderDevice->CreateTexture("rbed");
		pTexture->Load(strTexturePath.c_str());

		return true;
	}

	bool Smt3DXView::CreateTools(void)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		SmtGroupToolFactory::CreateGroup3DTool(m_p3DViewCtrlTool,GroupTool3DType::GTT_3DViewControl);

		if (NULL == m_p3DViewCtrlTool)
			return false;

		if (m_p3DViewCtrlTool->Init(m_p3DRenderDevice,m_pScene,m_hWnd) != SMT_ERR_NONE)
			return false;

		m_p3DViewCtrlTool->SetActive();

		pLog->LogMessage("Init Group3DTools ok!");

		return true;
	}

	BOOL Smt3DXView::OnCommand(WPARAM wParam, LPARAM lParam)
	{
		// TODO: 在此添加专用代码和/或调用基类
		unsigned int unMsg =(wParam);
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

		return SmtXView::OnCommand(wParam,lParam);
	}
}