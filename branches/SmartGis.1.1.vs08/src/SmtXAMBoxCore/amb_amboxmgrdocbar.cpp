#include "StdAfx.h"
#include "amb_amboxmgrdocbar.h"
#include "amb_xambox.h"

namespace Smt_XAMBox
{
	/////////////////////////////////////////////////////////////////////////////
	// SmtAMBoxMgrDocBar

	BEGIN_MESSAGE_MAP(SmtAMBoxMgrDocBar, CBCGPOutlookBar)
		//{{AFX_MSG_MAP(SmtAMBoxMgrDocBar)
		//}}AFX_MSG_MAP
		//	ON_WM_CONTEXTMENU()
	END_MESSAGE_MAP()

	/////////////////////////////////////////////////////////////////////////////
	// CCatalogDockBar construction/destruction

	SmtAMBoxMgrDocBar::SmtAMBoxMgrDocBar()
	{
		// TODO: add one-time construction code here
		m_nToolBoxPage = -1;
	}

	SmtAMBoxMgrDocBar::~SmtAMBoxMgrDocBar()
	{
		vector<CWnd*>::iterator iter = m_vWndPtrs.begin();
		while(iter != m_vWndPtrs.end())
		{
			if (*iter)
			{
				(*iter)->DestroyWindow();
				SMT_SAFE_DELETE(*iter);
			}
			iter++;
		}

		m_vWndPtrs.clear();
	}

	void SmtAMBoxMgrDocBar::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/)
	{
		// TODO: 在此处添加消息处理程序代码
	}

	/////////////////////////////////////////////////////////////////////////////
	// SmtAMBoxMgrDocBar message handlers
	bool SmtAMBoxMgrDocBar::UpdateAMBoxs(void)
	{
		SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();
		if (pAModuleMgr)
		{
			for (int i = 0; i < pAModuleMgr->GetAModuleCount();i ++)
			{
				SmtAuxModule *pAModule = pAModuleMgr->GetAModule(i);
				CreateAMBox(pAModule,i+1);
			}
		}

		return true;
	}

	bool SmtAMBoxMgrDocBar::CreateAMBox(SmtAuxModule* pAModule,int nID)
	{
		if (NULL == pAModule)
			return false;
		 
		SmtXAMBox *pXAMBox = new SmtXAMBox(pAModule);
		if(!pXAMBox->Create(WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT|TVS_SHOWSELALWAYS , CRect(0,0,0,0),this,nID))
		{
			TRACE0("Failed to create amtree");
			return false;
		}

		//pXAMBox->InitCreate();

		pXAMBox->ModifyStyleEx(0,WS_EX_CLIENTEDGE);
		pXAMBox->UpdateAMBoxTree();

		AddWnd(pXAMBox,pAModule->GetName());

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtAMBoxMgrDocBar::AddWnd(CWnd* pWnd,CString strTitle)
	{
		if (pWnd == NULL)
		{
			return false;
		}

		m_vWndPtrs.push_back(pWnd);

		CBCGPOutlookWnd* pContainer = DYNAMIC_DOWNCAST (CBCGPOutlookWnd,GetUnderlinedWindow ());

		if (pContainer == NULL)
		{
			TRACE0("Cannot get outlook bar container\n");
			return false;
		}

		pContainer->AddControl(pWnd,strTitle,0, TRUE,CBRS_BCGP_FLOAT | CBRS_BCGP_AUTOHIDE | CBRS_BCGP_RESIZE);
		pWnd->ShowWindow(SW_SHOW);

		return true;
	}


}