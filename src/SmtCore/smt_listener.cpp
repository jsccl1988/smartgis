#include "smt_listener.h "
#include <algorithm>

#include "smt_listenermanager.h"

namespace Smt_Core
{
	SmtListener::SmtListener()
	{

	}

	SmtListener::~SmtListener()
	{

	}

	//////////////////////////////////////////////////////////////////////////
	const char* SmtListener::GetName() const
	{ 
		return m_szListenerName;
	}

	void SmtListener::SetName(const char*szName) 
	{ 
		strcpy(m_szListenerName,szName);
	}


	bool SmtListener::AppendFuncItems(const char *szFunc,long lFuncMsg,long lStyle)
	{
		bool bRet = true;
		if (lStyle & FIM_2DVIEW)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_v2DViewFuncItems);
		}

		if (lStyle & FIM_3DVIEW)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_v3DViewFuncItems);
		}

		if (lStyle & FIM_3DEXVIEW)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_v3DExViewFuncItems);
		}

		if (lStyle & FIM_MAPDOCCATALOG)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_vMDCatalogFuncItems);
		}

		if (lStyle & FIM_2DMFTOOLBAR)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_v2DToolBarFuncItems);
		}

		if (lStyle & FIM_3DMFTOOLBAR)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_v3DToolBarFuncItems);
		}

		if (lStyle & FIM_2DMFMENU)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_v2DMMenuFuncItems);
		}

		if (lStyle & FIM_3DMFMENU)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_v3DMMenuFuncItems);
		}

		if (lStyle & FIM_AUXMODULEBOX)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_vAMBoxFuncItems);
		}

		if (lStyle & FIM_AUXMODULETREE)
		{
			bRet &= AppendFuncItems(szFunc,lFuncMsg,m_vAMTreeFuncItems);
		}

		return bRet;
	}

	vSmtFuncItems SmtListener::GetFuncItems(SmtFuncItemStyle style)
	{
		vSmtFuncItems vTmpFuncItems;
		switch (style)
		{
		case FIM_2DVIEW:
			{
				vTmpFuncItems = m_v2DViewFuncItems;
				//copy(m_v2DViewFuncItems.begin(),m_v2DViewFuncItems.end(),vTmpFuncItems.begin());
			}
			break;
		case FIM_3DVIEW:
			{
				vTmpFuncItems = m_v3DViewFuncItems;
				//copy(m_v3DViewFuncItems.begin(),m_v3DViewFuncItems.end(),vTmpFuncItems.begin());
			}
			break;
		case FIM_3DEXVIEW:
			{
				vTmpFuncItems = m_v3DExViewFuncItems;
				//copy(m_v3DViewFuncItems.begin(),m_v3DViewFuncItems.end(),vTmpFuncItems.begin());
			}
			break;
		case FIM_MAPDOCCATALOG:
			{
				vTmpFuncItems = m_vMDCatalogFuncItems;
				//copy(m_vMDCatalogFuncItems.begin(),m_vMDCatalogFuncItems.end(),vTmpFuncItems.begin());
			}
			break;
		case FIM_2DMFTOOLBAR:
			{
				vTmpFuncItems = m_v2DToolBarFuncItems;
				//copy(m_v2DToolBarFuncItems.begin(),m_v2DToolBarFuncItems.end(),vTmpFuncItems.begin());
			}
			break;
		case FIM_3DMFTOOLBAR:
			{
				vTmpFuncItems = m_v3DToolBarFuncItems;
				//copy(m_v3DToolBarFuncItems.begin(),m_v3DToolBarFuncItems.end(),vTmpFuncItems.begin());
			}
			break;

		case FIM_2DMFMENU:
			{
				vTmpFuncItems = m_v2DMMenuFuncItems;
				//copy(m_v2DMMenuFuncItems.begin(),m_v2DMMenuFuncItems.end(),vTmpFuncItems.begin());
			}
			break;
		case FIM_3DMFMENU:
			{
				vTmpFuncItems = m_v3DMMenuFuncItems;
				//copy(m_v3DMMenuFuncItems.begin(),m_v3DMMenuFuncItems.end(),vTmpFuncItems.begin());
			}
			break;
		case FIM_AUXMODULEBOX:
			{
				vTmpFuncItems = m_vAMBoxFuncItems;
				//copy(m_vAMBoxFuncItems.begin(),m_vAMBoxFuncItems.end(),vTmpFuncItems.begin());
			}
			break;
		case FIM_AUXMODULETREE:
			{
				vTmpFuncItems = m_vAMTreeFuncItems;
				//copy(m_vAMTreeFuncItems.begin(),m_vAMTreeFuncItems.end(),vTmpFuncItems.begin());
			}
			break;
		}

		return vTmpFuncItems;
	}

	bool SmtListener::AppendFuncItems(const char *szFunc,long lFuncMsg,vSmtFuncItems &vFuncItems)
	{
		vSmtFuncItems::iterator iter = vFuncItems.begin();	
		while (iter != vFuncItems.end())
		{			
			if ((*iter).lMsg == lFuncMsg)
			{
				return false;
			}
			++iter;
		}

		SmtFuncItem funcItem;

		strcpy(funcItem.szName,szFunc);
		funcItem.lMsg = lFuncMsg;

		vFuncItems.push_back(funcItem);

		return true;
	}

	bool SmtListener::AppendMsg(long lFuncMsg)
	{
		vSmtMsgs::iterator iter =  find(m_vMsgs.begin(),m_vMsgs.end(),lFuncMsg);
		if (iter != m_vMsgs.end())
			return false;
		
		m_vMsgs.push_back(lFuncMsg);

		return true;
	}

	int SmtListener::Register()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::GetSingletonPtr();
		return pListenerMgr->RegisterListener(this);
	}

	int SmtListener::RegisterMsg()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::GetSingletonPtr();
		pListenerMgr->RegisterListenerMsg(this);

		return SMT_ERR_NONE;
	}

	int SmtListener::UnRegister()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::GetSingletonPtr();
		return pListenerMgr->RemoveListener(this);
	}

	int SmtListener::UnRegisterMsg()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::GetSingletonPtr();
		pListenerMgr->UnRegisterListenerMsg(this);

		return SMT_ERR_NONE;
	}

	int SmtListener::SetActive()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::GetSingletonPtr();
		pListenerMgr->SetActiveListener(this);

		return SMT_ERR_NONE;
	}
}