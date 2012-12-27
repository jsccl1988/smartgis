// Smt3DObjXCatalog.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtXCatalogCore.h"
#include "cata_3dobjxcatalog.h"

#include "cata_mapmgr.h"
#include "cata_scenemgr.h"
#include "gis_map.h"
#include "gis_sde.h"
#include "smt_api.h"
#include "sys_sysmanager.h"
#include "sde_datasourcemgr.h"
#include "gis_api.h"
#include "smt_logmanager.h"
#include "t_iatoolmanager.h"
#include "smt_msg.h"
#include "gt_defs.h"
#include "md3d_sphere.h"
#include "md3d_water.h"
#include "md3d_terrain.h"
#include "smt_api.h"

using namespace Smt_3DBase;
using namespace Smt_3DModel;

using namespace Smt_SDEDevMgr;
using namespace Smt_GIS;
using namespace Smt_Sys;


#include "DlgSelLayer.h"
#include "DlgCreateLayer.h"
// Smt3DObjXCatalog

namespace Smt_XCatalog
{
	IMPLEMENT_DYNAMIC(Smt3DObjXCatalog, SmtXCatalog)

		Smt3DObjXCatalog::Smt3DObjXCatalog()
	{
		m_hContexMenu = NULL;
	}

	Smt3DObjXCatalog::~Smt3DObjXCatalog()
	{
		;
	}

	BEGIN_MESSAGE_MAP(Smt3DObjXCatalog, CTreeCtrl)
		ON_WM_RBUTTONDOWN()
		ON_WM_LBUTTONDOWN()
		ON_WM_CREATE()

		ON_COMMAND(ID_3DOBJECT_MGR_VISIBLE, &Smt3DObjXCatalog::On3DObjMgrSetVisible)
		ON_COMMAND(ID_3DOBJECT_MGR_HIDE, &Smt3DObjXCatalog::On3DObjMgrSetHide)
		ON_COMMAND(ID_3DOBJECT_MGR_REMOVE, &Smt3DObjXCatalog::On3DObjMgrRemove)
		ON_COMMAND(ID_3DOBJECT_MGR_RECREATE_OCTTREE, &Smt3DObjXCatalog::On3DObjMgrReCreateSceneTree)
		
	END_MESSAGE_MAP()

	// Smt3DObjXCatalog 消息处理程序
	bool Smt3DObjXCatalog::InitCreate(void) 
	{ 
		SmtXCatalog::InitCreate();

		SmtSceneMgr *pSceneMgr = SmtSceneMgr::GetSingletonPtr();
		pSceneMgr->Register3DObjCatalog((void*)this);
		
		return true;
	}

	bool Smt3DObjXCatalog::EndDestory(void) 
	{ 
		return SmtXCatalog::EndDestory();
	}

	bool Smt3DObjXCatalog::CreateContexMenu()
	{
		m_hContexMenu = ::CreatePopupMenu();
		return SmtXCatalog::CreateContexMenu();
	}

	int Smt3DObjXCatalog::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (SmtXCatalog::OnCreate(lpCreateStruct) == -1)
			return -1;

		// TODO:  在此添加您专用的创建代码
#ifdef _DEBUG
		HINSTANCE   hInstance  =  ::GetModuleHandle("SmtXCatalogCoreD.dll");
#else
		HINSTANCE   hInstance  =  ::GetModuleHandle("SmtXCatalogCore.dll");
#endif

		m_imgList.Create(16,16,ILC_COLOR16|ILC_MASK,1,0);

		m_imgList.SetBkColor(RGB(255,255,255));

		m_imgList.Add(::LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON_OBJ_V)));
		m_imgList.Add(::LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON_OBJ_UV)));
		
		SetImageList(&m_imgList,TVSIL_NORMAL);

		return 0;
	}

	void Smt3DObjXCatalog::OnRButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		HTREEITEM hItem = HitTest(point,&m_nFlags); 
		if((hItem != NULL)&&(TVHT_ONITEM&nFlags)) 
		{ 
			SelectItem(hItem); 
		}
		else 
			return;

		HTREEITEM hParentItem=GetParentItem(hItem);
		if (hParentItem == m_hRoot || hItem == m_hRoot)
		{
			m_strSelObjName = GetItemText(hItem);
		}

		CMenu menuMapMgr;
		menuMapMgr.LoadMenu(IDR_MENU_3DMGR);

		CMenu* pMenu = NULL;
		pMenu = menuMapMgr.GetSubMenu(0);

		if (pMenu)
		{
			CPoint      menuPos;   
			GetCursorPos(&menuPos);  

			pMenu->TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,menuPos.x,menuPos.y,this);
			pMenu->Detach();
		}		

		SmtXCatalog::OnRButtonDown(nFlags, point);
	}

	void Smt3DObjXCatalog::OnLButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值

		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		HTREEITEM hItem = HitTest(point,&nFlags); 

		if((hItem != NULL)&&(TVHT_ONITEM&nFlags)) 
		{ 
			SelectItem(hItem); 
		}
		else 
			return;

		SmtXCatalog::OnLButtonDown(nFlags, point);
	}

	//////////////////////////////////////////////////////////////////////////
	bool Smt3DObjXCatalog::Update3DObjTree()
	{
		DeleteAllItems();
		SetRedraw(FALSE);
		SetTextColor(RGB(0,0,255));

		m_hRoot =InsertItem("三维对象");

		SmtSceneMgr *pSceneMgr = SmtSceneMgr::GetSingletonPtr();
		
		vSmt3DObjectPtrs v3DObjectPtrs;
		pSceneMgr->Get3DObjectPtrs(v3DObjectPtrs);

		vSmt3DObjectPtrs ::iterator iter = v3DObjectPtrs.begin();
		CString strIndex;
		int i = 0;
		while (iter != v3DObjectPtrs.end())
		{		
			strIndex.Format("%d",i++);
			HTREEITEM hItem = InsertItem(strIndex,0,1,m_hRoot);
			++iter;
		}

		Expand(m_hRoot,TVE_EXPAND);
		SetRedraw(TRUE);
		RedrawWindow();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void Smt3DObjXCatalog::On3DObjMgrSetVisible()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtSceneMgr *pSceneMgr = SmtSceneMgr::GetSingletonPtr();
		if (!m_strSelObjName.IsEmpty())
		{
			Smt3DObject*	p3DObject = NULL;
			p3DObject = pSceneMgr->Get3DObject(atoi(m_strSelObjName));
			if (NULL != p3DObject)
			{
				p3DObject->SetVisible(true);
			}
		}

		ReleaseCapture();
	}

	void Smt3DObjXCatalog::On3DObjMgrSetHide()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtSceneMgr *pSceneMgr = SmtSceneMgr::GetSingletonPtr();
		if (!m_strSelObjName.IsEmpty())
		{
			Smt3DObject*	p3DObject = NULL;
			p3DObject = pSceneMgr->Get3DObject(atoi(m_strSelObjName));
			if (NULL != p3DObject)
			{
				p3DObject->SetVisible(false);
			}
		}

		ReleaseCapture();
	}

	void Smt3DObjXCatalog::On3DObjMgrRemove()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtSceneMgr *pSceneMgr = SmtSceneMgr::GetSingletonPtr();
		if (!m_strSelObjName.IsEmpty())
		{
			pSceneMgr->Remove3DObject(atoi(m_strSelObjName));
		}
	
		ReleaseCapture();
	}

	void Smt3DObjXCatalog::On3DObjMgrReCreateSceneTree()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtSceneMgr *pSceneMgr = SmtSceneMgr::GetSingletonPtr();
		pSceneMgr->CreateOctTreeSceneMgr();
	
		ReleaseCapture();
	}
}