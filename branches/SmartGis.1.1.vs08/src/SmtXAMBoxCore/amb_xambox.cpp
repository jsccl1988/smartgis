// SmtXAMBox.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtXAMBoxCore.h"
#include "amb_xambox.h"
#include "smt_msg_def.h"

// SmtXAMBox
namespace Smt_XAMBox
{
	IMPLEMENT_DYNAMIC(SmtXAMBox, CTreeCtrl)

	SmtXAMBox::SmtXAMBox(SmtAuxModule *pAModule):m_pAModule(pAModule)
		,m_hContexMenu(NULL)
		,m_hRoot(NULL)
	{

	}

	SmtXAMBox::~SmtXAMBox()
	{
		m_pAModule = NULL;
	}


	BEGIN_MESSAGE_MAP(SmtXAMBox, CTreeCtrl)
		ON_WM_CREATE()
		ON_WM_LBUTTONDOWN()
		ON_WM_LBUTTONUP()
		ON_WM_RBUTTONDOWN()
	END_MESSAGE_MAP()


	// SmtXAMBox 消息处理程序
	BOOL SmtXAMBox::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
	{
		// TODO: 在此添加专用代码和/或调用基类

		return CTreeCtrl::Create(dwStyle, rect, pParentWnd, nID);
	}

	int SmtXAMBox::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (CTreeCtrl::OnCreate(lpCreateStruct) == -1)
			return -1;

		// TODO:  在此添加您专用的创建代码
		if (!InitCreate())
		{
			return -1;
		}


		return 0;
	}

	bool SmtXAMBox::InitCreate(void) 
	{ 
		//AFX_MANAGE_STATE(AfxGetStaticModuleState());

#ifdef _DEBUG
		HINSTANCE   hInstance  =  ::GetModuleHandle("SmtXAMBoxCoreD.dll");
#else
		HINSTANCE   hInstance  =  ::GetModuleHandle("SmtXAMBoxCore.dll");
#endif

		m_imgList.Create(16,16,ILC_COLOR16|ILC_MASK,1,0);

		m_imgList.SetBkColor(RGB(255,255,255));

		m_imgList.Add(::LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON_AMBOX)));
		m_imgList.Add(::LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON_AMBOX_ITEM)));

		SetImageList(&m_imgList,TVSIL_NORMAL);

		m_vFuncItems = m_pAModule->GetFuncItems(FIM_AUXMODULEBOX);

		return (CreateContexMenu() && UpdateAMBoxTree());
	}

	bool SmtXAMBox::EndDestory(void) 
	{ 
		::DestroyMenu(m_hContexMenu);

		return true;
	}

	bool SmtXAMBox::CreateContexMenu(void)
	{
		CMenu menuDSMgr;
		menuDSMgr.LoadMenu(IDR_MENU_XMBOXMGR);
		m_hContexMenu = menuDSMgr.GetSafeHmenu();

		return true;
	}

	void SmtXAMBox::OnRButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值

		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		HTREEITEM hItem = HitTest(point,   &nFlags); 
		if((hItem != NULL)&&(TVHT_ONITEM&nFlags)) 
		{ 
			SelectItem(hItem); 
		}
		else 
			return;

		CString	strFuncItem;
		strFuncItem= GetItemText(hItem);	

		CMenu menuMapMgr;
		menuMapMgr.LoadMenu(IDR_MENU_XMBOXMGR);

		CMenu* pMenu = NULL;
		pMenu = menuMapMgr.GetSubMenu(0);
		
		if (pMenu)
		{
			CPoint menuPos;
			GetCursorPos(&menuPos); 
			pMenu->TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,menuPos.x,menuPos.y,this);
			menuMapMgr.Detach();
		}
	}

	void SmtXAMBox::OnLButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
	}

	void SmtXAMBox::OnLButtonUp(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		static SmtListenerMsg param;
		param.hSrcWnd = m_hWnd;

		HTREEITEM hItem = HitTest(point,   &nFlags); 
		if((hItem != NULL)&&(TVHT_ONITEM&nFlags)) 
		{ 
			SelectItem(hItem); 
		}
		else 
			return;

		CString	strFuncItem;
		strFuncItem= GetItemText(hItem);	

		vSmtFuncItems::iterator iter = m_vFuncItems.begin();	
		while (iter != m_vFuncItems.end())
		{
			if (strcmp((*iter).szName,strFuncItem) == 0)
			{
				m_pAModule->Notify((*iter).lMsg,param);
				return ;
			}

			++iter;
		}
	}

	bool SmtXAMBox::UpdateAMBoxTree(void)
	{
		if (NULL == m_pAModule)
			return false;
		 
		SetRedraw(FALSE);
		DeleteAllItems();
		SetTextColor(RGB(0,0,255));

		m_hRoot =InsertItem(m_pAModule->GetName(),0,0,TVI_ROOT);

		vSmtFuncItems::iterator iter = m_vFuncItems.begin();	
		while (iter != m_vFuncItems.end())
		{		
			HTREEITEM hItem = InsertItem((*iter).szName,1,1,m_hRoot);
			++iter;
		}

		Expand(m_hRoot,TVE_EXPAND);

		SetRedraw(TRUE);

		RedrawWindow();	

		return true;
	}
}

