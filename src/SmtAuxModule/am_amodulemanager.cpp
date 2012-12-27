#include "am_amodulemanager.h"
#include "smt_msg.h"
#include "am_msg.h"
#include <algorithm>

namespace Smt_AM
{
	SmtAModuleManager* SmtAModuleManager::m_pSingleton = NULL;

	SmtAModuleManager* SmtAModuleManager::GetSingletonPtr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtAModuleManager();
		}
		return m_pSingleton;
	}

	void SmtAModuleManager::DestoryInstance(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		SMT_SAFE_DELETE(m_pSingleton);
	}
	//////////////////////////////////////////////////////////////////////////

	SmtAModuleManager::SmtAModuleManager(void)
	{ 
		m_pActiveAModules = NULL;
	}

	SmtAModuleManager::~SmtAModuleManager(void)
	{
		RemoveAllAModule();
	}

	long SmtAModuleManager::Notify(SmtAuxModule *pAModule,long lMsg,SmtListenerMsg &param)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		if (pAModule == SMT_AM_MSG_INVALID)
		{
			return SMT_ERR_INVALID_PARAM;
		}
		
		if (pAModule == SMT_AM_MSG_BROADCAST)
		{
			mapMsgToPtr::iterator mapIter ;
			mapIter = m_mapMsgToAModules.find(lMsg);
			if (mapIter != m_mapMsgToAModules.end())
			{
				SmtAuxModule *pFindAModule = (SmtAuxModule *)mapIter->second;
				if (pFindAModule)
				{
					pFindAModule->Notify(lMsg,param);
				}
			}
			else
			{
				//Ö§³ÖÐÞ¸Älist
				vSmtAModulePtrs  vDoneList;
				vSmtAModulePtrs ::iterator it = m_vAModulePtrs.begin(); 
				while(it  !=  m_vAModulePtrs.end()) 
				{ 
					if(find(vDoneList.begin(),vDoneList.end(),*it)  ==  vDoneList.end()) 
					{ 
						param.bModify = false;
						vDoneList.push_back(*it); 
						(*it++)-> Notify(lMsg,param); 
						if(param.bModify )
						{ 
							it  =  m_vAModulePtrs.begin();
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
			pAModule->Notify(lMsg,param);
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtAModuleManager::RegisterAModule(SmtAuxModule *pAModule)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		vSmtAModulePtrs::iterator i = m_vAModulePtrs.begin();	

		while (i != m_vAModulePtrs.end())
		{			
			if ((*i) == pAModule)
			{
				return false;
			}
			++i;
		}

		m_vAModulePtrs.push_back(pAModule);

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtAModuleManager::RemoveAModule(SmtAuxModule*pAModule)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		vSmtAModulePtrs::iterator i = m_vAModulePtrs.begin();	
		while (i != m_vAModulePtrs.end())
		{			
			if ((*i) == pAModule)
			{
				m_vAModulePtrs.erase(i);	
				break;
			}
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		return true;
	}

	long SmtAModuleManager::RemoveAllAModule(void)
	{
		m_vAModulePtrs.clear();

		return SMT_ERR_NONE;
	}

	SmtAuxModule* SmtAModuleManager::GetAModule(int index)
	{
		if ( index < 0 || index >= m_vAModulePtrs.size() )
			return NULL;

		return m_vAModulePtrs.at(index);
	}

	const SmtAuxModule* SmtAModuleManager::GetAModule(int index) const
	{
		if ( index < 0 || index >= m_vAModulePtrs.size() )
			return NULL;

		return m_vAModulePtrs.at(index);
	}

	long SmtAModuleManager::RegisterAModuleMsg(SmtAuxModule *pAModule)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		if (pAModule == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pAModule->GetMsgs();

		mapMsgToPtr::iterator mapIter = m_mapMsgToAModules.begin();
		vSmtMsgs::iterator iter = vMsgs.begin();	
		while (iter != vMsgs.end())
		{		
			if (m_mapMsgToAModules.find((*iter)) == m_mapMsgToAModules.end())
			{
				m_mapMsgToAModules.insert(pairMsgToPtr((*iter),(void*)pAModule));
			}
			++iter;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtAModuleManager::UnRegisterAModuleMsg(SmtAuxModule *pAModule)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		if (pAModule == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pAModule->GetMsgs();

		mapMsgToPtr::iterator mapIter = m_mapMsgToAModules.begin();
		vSmtMsgs::iterator iter = vMsgs.begin();	
		
		while (iter != vMsgs.end())
		{		
			if ( ( mapIter = m_mapMsgToAModules.find((*iter)) ) != m_mapMsgToAModules.end())
			{
				m_mapMsgToAModules.erase(mapIter);
			}
			++iter;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		return SMT_ERR_NONE;
	}
}