#include "t_iatoolmanager.h"
#include "t_msg.h"
#include <algorithm>

namespace Smt_IATool
{
	SmtIAToolManager* SmtIAToolManager::m_pSingleton = NULL;

	SmtIAToolManager* SmtIAToolManager::GetSingletonPtr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtIAToolManager();
		}
		return m_pSingleton;
	}

	void SmtIAToolManager::DestoryInstance(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		SMT_SAFE_DELETE(m_pSingleton);
	}
	//////////////////////////////////////////////////////////////////////////

	SmtIAToolManager::SmtIAToolManager(void)
	{ 
		m_pActiveIATool = NULL;
	}

	SmtIAToolManager::~SmtIAToolManager(void)
	{
		RemoveAllIATool();
	}

	long SmtIAToolManager::Notify(SmtIATool *pIATool,long lMsg,SmtListenerMsg &param)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		if (pIATool == SMT_IATOOL_MSG_INVALID)
		{
			return SMT_ERR_INVALID_PARAM;
		}
		
		if (pIATool == SMT_IATOOL_MSG_BROADCAST)
		{
			mapMsgToPtr::iterator mapIter ;
			mapIter = m_mapMsgToIATools.find(lMsg);
			if (mapIter != m_mapMsgToIATools.end())
			{
				SmtIATool *pFindIATool = (SmtIATool *)mapIter->second;
				if (pFindIATool)
				{
					pFindIATool->Notify(SMT_MSG_KEY_LMSG(lMsg),param);
				}
			}
			else
			{
				//Ö§³ÖÐÞ¸Älist
				vSmtIAToolPtrs  vDoneList;
				vSmtIAToolPtrs ::iterator it = m_vIAToolPtrs.begin(); 
				while(it  !=  m_vIAToolPtrs.end()) 
				{ 
					if(find(vDoneList.begin(),vDoneList.end(),*it)  ==  vDoneList.end()) 
					{ 
						param.bModify = false;
						vDoneList.push_back(*it); 
						(*it++)-> Notify(SMT_MSG_KEY_LMSG(lMsg),param); 
						if(param.bModify )
						{ 
							it  =  m_vIAToolPtrs.begin();
							continue; 
						} 
					} 
					else 
						++it; 
				}
			}
		}
		else
		{
			pIATool->Notify(lMsg,param);
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtIAToolManager::RegisterIATool(SmtIATool *pIATool)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		vSmtIAToolPtrs::iterator i = m_vIAToolPtrs.begin();	

		while (i != m_vIAToolPtrs.end())
		{			
			if ((*i) == pIATool)
			{
				return false;
			}
			++i;
		}

		m_vIAToolPtrs.push_back(pIATool);

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtIAToolManager::RemoveIATool(SmtIATool*pIATool)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		vSmtIAToolPtrs::iterator i = m_vIAToolPtrs.begin();	
		while (i != m_vIAToolPtrs.end())
		{			
			if ((*i) == pIATool)
			{
				m_vIAToolPtrs.erase(i);	
				if (pIATool == m_pActiveIATool)
				{
					m_pActiveIATool = NULL;
				}
				break;
			}
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return true;
	}

	long SmtIAToolManager::RemoveAllIATool(void)
	{
		m_vIAToolPtrs.clear();
		m_pActiveIATool = NULL;
		return SMT_ERR_NONE;
	}

	SmtIATool* SmtIAToolManager::GetIATool(int index)
	{
		if ( index < 0 || index >= m_vIAToolPtrs.size() )
			return NULL;

		return m_vIAToolPtrs.at(index);
	}

	const SmtIATool* SmtIAToolManager::GetIATool(int index) const
	{
		if ( index < 0 || index >= m_vIAToolPtrs.size() )
			return NULL;

		return m_vIAToolPtrs.at(index);
	}

	long SmtIAToolManager::RegisterIAToolMsg(SmtIATool *pIATool)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		if (pIATool == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pIATool->GetMsgs();
		
		mapMsgToPtr::iterator mapIter = m_mapMsgToIATools.begin();
		vSmtMsgs::iterator iter = vMsgs.begin();	
		while (iter != vMsgs.end())
		{		
			if (m_mapMsgToIATools.find((*iter)) == m_mapMsgToIATools.end())
			{
				m_mapMsgToIATools.insert(pairMsgToPtr((*iter),(void*)pIATool));
			}
			++iter;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtIAToolManager::UnRegisterIAToolMsg(SmtIATool *pIATool)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		if (pIATool == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pIATool->GetMsgs();

		mapMsgToPtr::iterator mapIter = m_mapMsgToIATools.begin();
		vSmtMsgs::iterator iter = vMsgs.begin();	

		while (iter != vMsgs.end())
		{		
			if ( ( mapIter = m_mapMsgToIATools.find((*iter)) ) != m_mapMsgToIATools.end())
			{
				m_mapMsgToIATools.erase(mapIter);
			}
			++iter;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}
}