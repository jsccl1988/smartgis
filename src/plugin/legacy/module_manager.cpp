#include <mutex>
#include "plugin/legacy/module_manager.h"
#include "base/core/msg.h"
#include "plugin/legacy/plugin_msg.h"
#include <algorithm>

namespace plugin
{
	SmtAModuleManager* SmtAModuleManager::m_pSingleton = NULL;

	SmtAModuleManager* SmtAModuleManager::get_singleton_ptr(void)
	{
		static std::mutex cslock;
		std::lock_guard<std::mutex> scope(cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtAModuleManager();
		}
		return m_pSingleton;
	}

	void SmtAModuleManager::destroy_instance(void)
	{
		static std::mutex cslock;
		std::lock_guard<std::mutex> scope(cslock);

		SMT_SAFE_DELETE(m_pSingleton);
	}
	//////////////////////////////////////////////////////////////////////////

	SmtAModuleManager::SmtAModuleManager(void)
	{ 
		m_pActiveAModules = NULL;
	}

	SmtAModuleManager::~SmtAModuleManager(void)
	{
		remove_all_a_module();
	}

	long SmtAModuleManager::notify(SmtAuxModule *pAModule,long lMsg,SmtListenerMsg &param)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
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
					pFindAModule->notify(lMsg,param);
				}
			}
			else
			{
				//֧���޸�list
				vSmtAModulePtrs  vDoneList;
				vSmtAModulePtrs ::iterator it = m_vAModulePtrs.begin(); 
				while(it  !=  m_vAModulePtrs.end()) 
				{ 
					if(find(vDoneList.begin(),vDoneList.end(),*it)  ==  vDoneList.end()) 
					{ 
						param.bModify = false;
						vDoneList.push_back(*it); 
						(*it++)->notify(lMsg,param); 
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
			pAModule->notify(lMsg,param);
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtAModuleManager::register_a_module(SmtAuxModule *pAModule)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
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
		m_cslock.unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtAModuleManager::remove_a_module(SmtAuxModule*pAModule)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
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
		m_cslock.unlock();
#endif

		return true;
	}

	long SmtAModuleManager::remove_all_a_module(void)
	{
		m_vAModulePtrs.clear();

		return SMT_ERR_NONE;
	}

	SmtAuxModule* SmtAModuleManager::get_a_module(int index)
	{
		if ( index < 0 || index >= m_vAModulePtrs.size() )
			return NULL;

		return m_vAModulePtrs.at(index);
	}

	const SmtAuxModule* SmtAModuleManager::get_a_module(int index) const
	{
		if ( index < 0 || index >= m_vAModulePtrs.size() )
			return NULL;

		return m_vAModulePtrs.at(index);
	}

	long SmtAModuleManager::register_a_module_msg(SmtAuxModule *pAModule)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

		if (pAModule == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pAModule->get_msgs();

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
		m_cslock.unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtAModuleManager::unregister_a_module_msg(SmtAuxModule *pAModule)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

		if (pAModule == NULL)
			return SMT_ERR_INVALID_PARAM;

		vSmtMsgs vMsgs = pAModule->get_msgs();

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
		m_cslock.unlock();
#endif

		return SMT_ERR_NONE;
	}
}