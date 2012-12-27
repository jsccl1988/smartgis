/*
File:    am_amodulemanager.h

Desc:    SmartGis AModule manager,线程安全

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_AMODULE_MGR_H
#define _SMT_AMODULE_MGR_H

#include "smt_core.h"
#include "am_amodule.h"
#include "smt_cslock.h"

#include <map>

namespace Smt_AM
{
	typedef map<string,SmtAuxModule>	SmtAuxModuleMap;

	class SMT_EXPORT_CLASS SmtAModuleManager
	{
	public:
		virtual~SmtAModuleManager(void);

	public:
		static SmtAModuleManager*		GetSingletonPtr(void);
		static void						DestoryInstance(void);

	public:
		long							Notify(SmtAuxModule *pAModule,long lMsg,SmtListenerMsg &param);

		long							RegisterAModule(SmtAuxModule *pAModule);
		long							RemoveAModule(SmtAuxModule*pAModule);
		long							RemoveAllAModule(void);

		void							SetActiveAModule(SmtAuxModule*pAModule) {m_pActiveAModules = pAModule;}
		SmtAuxModule*					GetActiveAModule(void) {return m_pActiveAModules;}
		const SmtAuxModule*				GetActiveAModule(void) const{return m_pActiveAModules;}

		int								GetAModuleCount(void) const{return m_vAModulePtrs.size();}
		SmtAuxModule*					GetAModule(int index);
		const SmtAuxModule*				GetAModule(int index) const;

		long							RegisterAModuleMsg(SmtAuxModule *pAModule);
		long							UnRegisterAModuleMsg(SmtAuxModule *pAModule);

	protected:
#ifdef SMT_THREAD_SAFE
		SmtCSLock						m_cslock;										//多线程安全
#endif
		vSmtAModulePtrs					m_vAModulePtrs;
		SmtAuxModule					*m_pActiveAModules;
		mapMsgToPtr						m_mapMsgToAModules;

	private:
		SmtAModuleManager(void);
		static SmtAModuleManager*		m_pSingleton;
	};
}

#if !defined(Export_SmtAuxModule)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtAuxModuleD.lib")
#       else
#          pragma comment(lib,"SmtAuxModule.lib")
#	    endif  
#endif

#endif //_SMT_AMODULE_MGR_H