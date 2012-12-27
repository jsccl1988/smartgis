#include "smt_listenermanager.h"
#include "smt_msg.h"
#include <algorithm>

namespace Smt_Core
{
	SmtListenerManager* SmtListenerManager::m_pSingleton = NULL;

	SmtListenerManager* SmtListenerManager::GetSingletonPtr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtListenerManager();
		}

		return m_pSingleton;
	}

	void SmtListenerManager::DestoryInstance(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		SMT_SAFE_DELETE(m_pSingleton);
	}
	//////////////////////////////////////////////////////////////////////////

	SmtListenerManager::SmtListenerManager(void)
	{ 
		m_pActiveListener = NULL;
	}

	SmtListenerManager::~SmtListenerManager(void)
	{
		RemoveAllListener();
	}

	long SmtListenerManager::Notify(SmtListener *pListener,long lMsg,SmtListenerMsg &param)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		if (pListener == SMT_LISTENER_MSG_INVALID)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		if (pListener == SMT_LISTENER_MSG_BROADCAST)
		{
			mapMsgToPtr::iterator mapIter ;
			mapIter = m_mapMsgToListeners.find(lMsg);
			if (mapIter != m_mapMsgToListeners.end())
			{
				SmtListener *pFindListener = (SmtListener *)mapIter->second;
				if (pFindListener)
				{
					pFindListener->Notify(lMsg,param);
				}
			}
			else
			{
				//Ö§³ÖÐÞ¸Älist
				vSmtListenerPtrs  vDoneList;
				vSmtListenerPtrs ::iterator it = m_vListenerPtrs.begin(); 
				while(it  !=  m_vListenerPtrs.end()) 
				{ 
					if(find(vDoneList.begin(),vDoneList.end(),*it)  ==  vDoneList.end()) 
					{ 
						param.bModify = false;
						vDoneList.push_back(*it); 
						(*it++)-> Notify(lMsg,param); 
						if(param.bModify )
						{ 
							it  =  m_vListenerPtrs.begin();
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
			pListener->Notify(lMsg,param);
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtListenerManager::RegisterListener(SmtListener *pListener)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		vSmtListenerPtrs::iterator i = m_vListenerPtrs.begin();	

		while (i != m_vListenerPtrs.end())
		{			
			if ((*i) == pListener)
			{
				return false;
			}
			++i;
		}

		m_vListenerPtrs.push_back(pListener);

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtListenerManager::RemoveListener(SmtListener*pListener)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		vSmtListenerPtrs::iterator i = m_vListenerPtrs.begin();	
		while (i != m_vListenerPtrs.end())
		{			
			if ((*i) == pListener)
			{
				m_vListenerPtrs.erase(i);	
				break;
			}
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		return true;
	}

	long SmtListenerManager::RemoveAllListener(void)
	{
		m_vListenerPtrs.clear();

		return SMT_ERR_NONE;
	}

	SmtListener* SmtListenerManager::GetListener(int index)
	{
		if ( index < 0 || index >= m_vListenerPtrs.size() )
			return NULL;

		return m_vListenerPtrs.at(index);
	}

	const SmtListener* SmtListenerManager::GetListener(int index) const
	{
		if ( index < 0 || index >= m_vListenerPtrs.size() )
			return NULL;

		return m_vListenerPtrs.at(index);
	}

	long SmtListenerManager::RegisterListenerMsg(SmtListener *pListener)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		if (pListener == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pListener->GetMsgs();

		mapMsgToPtr::iterator mapIter = m_mapMsgToListeners.begin();
		vSmtMsgs::iterator iter = vMsgs.begin();	
		while (iter != vMsgs.end())
		{		
			if (m_mapMsgToListeners.find((*iter)) == m_mapMsgToListeners.end())
			{
				m_mapMsgToListeners.insert(pairMsgToPtr((*iter),(void*)pListener));
			}
			++iter;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtListenerManager::UnRegisterListenerMsg(SmtListener *pListener)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		if (pListener == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pListener->GetMsgs();

		mapMsgToPtr::iterator mapIter = m_mapMsgToListeners.begin();
		vSmtMsgs::iterator iter = vMsgs.begin();	

		while (iter != vMsgs.end())
		{		
			if ( ( mapIter = m_mapMsgToListeners.find((*iter)) ) != m_mapMsgToListeners.end())
			{
				m_mapMsgToListeners.erase(mapIter);
			}
			++iter;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		return SMT_ERR_NONE;
	}
}