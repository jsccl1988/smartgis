/*
File:    smt_listenermanager.h

Desc:    SmartGis listener manager,线程安全

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_LISTENER_MGR_H
#define _SMT_LISTENER_MGR_H

#include "smt_core.h"
#include "smt_listener.h"
#include "smt_cslock.h"

#include <map>

namespace Smt_Core
{
	typedef map<string,SmtListener>		SmtListenerMap;

	class SMT_EXPORT_CLASS SmtListenerManager
	{
	public:
		virtual~SmtListenerManager(void);

	public:
		static SmtListenerManager*		GetSingletonPtr(void);
		static void						DestoryInstance(void);

	public:
		long							Notify(SmtListener *pListener,long lMsg,SmtListenerMsg &param);

		long							RegisterListener(SmtListener *pListener);
		long							RemoveListener(SmtListener*pListener);
		long							RemoveAllListener(void);

		void							SetActiveListener(SmtListener*pListener) {m_pActiveListener = pListener;}
		SmtListener*					GetActiveListener(void) {return m_pActiveListener;}
		const SmtListener*				GetActiveListener(void) const{return m_pActiveListener;}

		int								GetListenerCount(void) const{return m_vListenerPtrs.size();}
		SmtListener*					GetListener(int index);
		const SmtListener*				GetListener(int index) const;

		long							RegisterListenerMsg(SmtListener *pListener);
		long							UnRegisterListenerMsg(SmtListener *pListener);

	protected:
#ifdef SMT_THREAD_SAFE
		SmtCSLock						m_cslock;										//多线程安全
#endif
		vSmtListenerPtrs				m_vListenerPtrs;
		SmtListener						*m_pActiveListener;
		mapMsgToPtr						m_mapMsgToListeners;

	private:
		SmtListenerManager(void);
		static SmtListenerManager*		m_pSingleton;
	};
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_LISTENER_MGR_H