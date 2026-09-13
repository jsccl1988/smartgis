#include "base/core/listenermanager.h"
#include "base/core/msg.h"
#include <algorithm>

namespace base
{
	SmtListenerManager* SmtListenerManager::m_pSingleton = NULL;

	SmtListenerManager* SmtListenerManager::get_singleton_ptr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtListenerManager();
		}

		return m_pSingleton;
	}

	void SmtListenerManager::destroy_instance(void)
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
		remove_all_listener();
	}

	long SmtListenerManager::notify(SmtListener *pListener,long lMsg,SmtListenerMsg &param)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
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
					pFindListener->notify(lMsg,param);
				}
			}
			else
			{
				//֧���޸�list
				vSmtListenerPtrs  vDoneList;
				vSmtListenerPtrs ::iterator it = m_vListenerPtrs.begin(); 
				while(it  !=  m_vListenerPtrs.end()) 
				{ 
					if(find(vDoneList.begin(),vDoneList.end(),*it)  ==  vDoneList.end()) 
					{ 
						param.bModify = false;
						vDoneList.push_back(*it); 
						(*it++)-> notify(lMsg,param); 
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
			pListener->notify(lMsg,param);
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtListenerManager::register_listener(SmtListener *pListener)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
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
		m_cslock.unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtListenerManager::remove_listener(SmtListener*pListener)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
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
		m_cslock.unlock();
#endif

		return true;
	}

	long SmtListenerManager::remove_all_listener(void)
	{
		m_vListenerPtrs.clear();

		return SMT_ERR_NONE;
	}

	SmtListener* SmtListenerManager::get_listener(int index)
	{
		if ( index < 0 || index >= m_vListenerPtrs.size() )
			return NULL;

		return m_vListenerPtrs.at(index);
	}

	const SmtListener* SmtListenerManager::get_listener(int index) const
	{
		if ( index < 0 || index >= m_vListenerPtrs.size() )
			return NULL;

		return m_vListenerPtrs.at(index);
	}

	long SmtListenerManager::register_listener_msg(SmtListener *pListener)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

		if (pListener == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pListener->get_msgs();

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
		m_cslock.unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtListenerManager::unregister_listener_msg(SmtListener *pListener)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

		if (pListener == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pListener->get_msgs();

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
		m_cslock.unlock();
#endif

		return SMT_ERR_NONE;
	}
}