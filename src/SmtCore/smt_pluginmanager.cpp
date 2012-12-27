#include "smt_pluginmanager.h"
#include "smt_filesys.h"
#include "smt_logmanager.h"

namespace Smt_Core
{
	extern const string g_strPluginLibLog;
	SmtPluginManager* SmtPluginManager::m_pSingleton = NULL;

	SmtPluginManager::SmtPluginManager(void)
	{
		SmtLogManager::GetSingletonPtr()->CreateLog(g_strPluginLibLog.c_str());
	}

	SmtPluginManager::~SmtPluginManager(void)
	{
	   UnLoadAllPlugin();
	}

	SmtPluginManager* SmtPluginManager::GetSingletonPtr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtPluginManager();
		}
		return m_pSingleton;
	}

	void SmtPluginManager::DestoryInstance(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////

	void SmtPluginManager::LoadAllPlugin(const char * filePath)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

        //get all plugins in file 
		vSmtFileInfos vFileInfos;
		SmtFileSystem fileSys;
		fileSys.SetCurrentDir(filePath);
		fileSys.SearchCurrentDir("*.am",false);
		vFileInfos = fileSys.GetFileInfos();

		//load plugins
		vSmtFileInfos::iterator i = vFileInfos.begin() ;

		while (i != vFileInfos.end())
		{
			LoadPlugin((*i).szName,(*i).szPath);
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		StartAllPlugin();
	}

	void SmtPluginManager::UnLoadAllPlugin()
	{
		StopAllPlugin();

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		// Unload & delete resources in turn
		for( vSmtPluginPtrs::iterator it = m_vPluginPtrs.begin(); it != m_vPluginPtrs.end(); ++it )
		{
			(*it)->UnLoad();
			SMT_SAFE_DELETE( *it);
		}

		// Empty the list
		m_vPluginPtrs.clear();

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
	}

	SmtPlugin* SmtPluginManager::LoadPlugin(const char * name,const char * path)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		SmtPlugin* pPlugin = NULL;

		vSmtPluginPtrs::iterator i = m_vPluginPtrs.begin();	
		while (i != m_vPluginPtrs.end())
		{			
			if (strcmp((*i)->GetName(),name) == 0)
			{
				pPlugin = *i;
				return pPlugin;
			}
			++i;
		}

		pPlugin = new SmtPlugin(name,path);
		if(pPlugin->Load())
			m_vPluginPtrs.push_back(pPlugin);
		else
			SMT_SAFE_DELETE(pPlugin);

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return pPlugin;
	}

	void SmtPluginManager::UnLoadPlugin(const char * name)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		SmtPlugin* pPlugin = NULL;

		vSmtPluginPtrs::iterator i = m_vPluginPtrs.begin();	
		while (i != m_vPluginPtrs.end())
		{			
			if (strcmp((*i)->GetName(),name) == 0)
			{
				pPlugin = *i;
				pPlugin->UnLoad();
				SMT_SAFE_DELETE(pPlugin);
				m_vPluginPtrs.erase(i);	
				break;
			}
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
	}

	void SmtPluginManager::StartAllPlugin()
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		for( vSmtPluginPtrs::iterator it = m_vPluginPtrs.begin(); it != m_vPluginPtrs.end(); ++it )
		{
			(*it)->StartPlugin();
		}
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
	}

	void SmtPluginManager::StopAllPlugin()
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		for( vSmtPluginPtrs::iterator it = m_vPluginPtrs.begin(); it != m_vPluginPtrs.end(); ++it )
		{
			(*it)->StopPlugin();
		}
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
	}
}