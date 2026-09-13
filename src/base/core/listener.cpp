#include "base/core/listener.h"
#include <algorithm>

#include "base/core/listenermanager.h"

namespace base
{
	SmtListener::SmtListener()
	{

	}

	SmtListener::~SmtListener()
	{

	}

	//////////////////////////////////////////////////////////////////////////
	const char* SmtListener::get_name() const
	{ 
		return m_szListenerName;
	}

	void SmtListener::set_name(const char*szName) 
	{ 
		strcpy(m_szListenerName,szName);
	}


	bool SmtListener::append_func_items(const char *szFunc,long lFuncMsg,long lStyle)
	{
		bool bRet = true;
		if (lStyle & FIM_2DVIEW)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_v2DViewFuncItems);
		}

		if (lStyle & FIM_3DVIEW)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_v3DViewFuncItems);
		}

		if (lStyle & FIM_3DEXVIEW)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_v3DExViewFuncItems);
		}

		if (lStyle & FIM_MAPDOCCATALOG)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_vMDCatalogFuncItems);
		}

		if (lStyle & FIM_2DMFTOOLBAR)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_v2DToolBarFuncItems);
		}

		if (lStyle & FIM_3DMFTOOLBAR)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_v3DToolBarFuncItems);
		}

		if (lStyle & FIM_2DMFMENU)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_v2DMMenuFuncItems);
		}

		if (lStyle & FIM_3DMFMENU)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_v3DMMenuFuncItems);
		}

		if (lStyle & FIM_AUXMODULEBOX)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_vAMBoxFuncItems);
		}

		if (lStyle & FIM_AUXMODULETREE)
		{
			bRet &= append_func_items(szFunc,lFuncMsg,m_vAMTreeFuncItems);
		}

		return bRet;
	}

	vSmtFuncItems SmtListener::get_func_items(SmtFuncItemStyle style)
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

	bool SmtListener::append_func_items(const char *szFunc,long lFuncMsg,vSmtFuncItems &vFuncItems)
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

	bool SmtListener::append_msg(long lFuncMsg)
	{
		vSmtMsgs::iterator iter =  find(m_vMsgs.begin(),m_vMsgs.end(),lFuncMsg);
		if (iter != m_vMsgs.end())
			return false;
		
		m_vMsgs.push_back(lFuncMsg);

		return true;
	}

	int SmtListener::register_()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::get_singleton_ptr();
		return pListenerMgr->register_listener(this);
	}

	int SmtListener::register_msg()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::get_singleton_ptr();
		pListenerMgr->register_listener_msg(this);

		return SMT_ERR_NONE;
	}

	int SmtListener::unregister()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::get_singleton_ptr();
		return pListenerMgr->remove_listener(this);
	}

	int SmtListener::unregister_msg()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::get_singleton_ptr();
		pListenerMgr->unregister_listener_msg(this);

		return SMT_ERR_NONE;
	}

	int SmtListener::set_active()
	{
		SmtListenerManager * pListenerMgr = SmtListenerManager::get_singleton_ptr();
		pListenerMgr->set_active_listener(this);

		return SMT_ERR_NONE;
	}
}