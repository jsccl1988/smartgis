#include "base/core/pluginmanager.h"
#include "base/core/filesys.h"
#include "base/core/logmanager.h"

namespace base
{
	extern const string g_strPluginLibLog;
	SmtPluginManager* SmtPluginManager::m_pSingleton = NULL;

	SmtPluginManager::SmtPluginManager(void)
	{
		SmtLogManager::get_singleton_ptr()->create_log(g_strPluginLibLog.c_str());
	}

	SmtPluginManager::~SmtPluginManager(void)
	{
	   unload_all_plugin();
	}

	SmtPluginManager* SmtPluginManager::get_singleton_ptr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtPluginManager();
		}
		return m_pSingleton;
	}

	void SmtPluginManager::destroy_instance(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////

	void SmtPluginManager::load_all_plugin(const char * filePath)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

        //get all plugins in file 
		vSmtFileInfos vFileInfos;
		SmtFileSystem fileSys;
		fileSys.set_current_dir(filePath);
		fileSys.search_current_dir("*.am",false);
		vFileInfos = fileSys.get_file_infos();

		//load plugins
		vSmtFileInfos::iterator i = vFileInfos.begin() ;

		while (i != vFileInfos.end())
		{
			load_plugin((*i).szName,(*i).szPath);
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
		start_all_plugin();
	}

	void SmtPluginManager::unload_all_plugin()
	{
		stop_all_plugin();

#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif
		// Unload & delete resources in turn
		for( vSmtPluginPtrs::iterator it = m_vPluginPtrs.begin(); it != m_vPluginPtrs.end(); ++it )
		{
			(*it)->unload();
			SMT_SAFE_DELETE( *it);
		}

		// Empty the list
		m_vPluginPtrs.clear();

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
	}

	SmtPlugin* SmtPluginManager::load_plugin(const char * name,const char * path)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif
		SmtPlugin* pPlugin = NULL;

		vSmtPluginPtrs::iterator i = m_vPluginPtrs.begin();	
		while (i != m_vPluginPtrs.end())
		{			
			if (strcmp((*i)->get_name(),name) == 0)
			{
				pPlugin = *i;
				return pPlugin;
			}
			++i;
		}

		pPlugin = new SmtPlugin(name,path);
		if(pPlugin->load())
			m_vPluginPtrs.push_back(pPlugin);
		else
			SMT_SAFE_DELETE(pPlugin);

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
		return pPlugin;
	}

	void SmtPluginManager::unload_plugin(const char * name)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif
		SmtPlugin* pPlugin = NULL;

		vSmtPluginPtrs::iterator i = m_vPluginPtrs.begin();	
		while (i != m_vPluginPtrs.end())
		{			
			if (strcmp((*i)->get_name(),name) == 0)
			{
				pPlugin = *i;
				pPlugin->unload();
				SMT_SAFE_DELETE(pPlugin);
				m_vPluginPtrs.erase(i);	
				break;
			}
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
	}

	void SmtPluginManager::start_all_plugin()
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif
		for( vSmtPluginPtrs::iterator it = m_vPluginPtrs.begin(); it != m_vPluginPtrs.end(); ++it )
		{
			(*it)->start_plugin();
		}
#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
	}

	void SmtPluginManager::stop_all_plugin()
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif
		for( vSmtPluginPtrs::iterator it = m_vPluginPtrs.begin(); it != m_vPluginPtrs.end(); ++it )
		{
			(*it)->stop_plugin();
		}
#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
	}
}