/*
File:    smt_listenermanager.h

Desc:    SmartGis listener manager,�̰߳�ȫ

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_LISTENER_MGR_H
#define _SMT_LISTENER_MGR_H

#include "base/core/core.h"
#include "base/core/listener.h"
#include <mutex>

#include <map>

namespace base
{
	typedef map<string,SmtListener>		SmtListenerMap;

	class CORE_EXPORT SmtListenerManager
	{
	public:
		virtual~SmtListenerManager(void);

	public:
		static SmtListenerManager*		get_singleton_ptr(void);
		static void						destroy_instance(void);

	public:
		long							notify(SmtListener *pListener,long lMsg,SmtListenerMsg &param);

		long							register_listener(SmtListener *pListener);
		long							remove_listener(SmtListener*pListener);
		long							remove_all_listener(void);

		void							set_active_listener(SmtListener*pListener) {m_pActiveListener = pListener;}
		SmtListener*					get_active_listener(void) {return m_pActiveListener;}
		const SmtListener*				get_active_listener(void) const{return m_pActiveListener;}

		int								get_listener_count(void) const{return m_vListenerPtrs.size();}
		SmtListener*					get_listener(int index);
		const SmtListener*				get_listener(int index) const;

		long							register_listener_msg(SmtListener *pListener);
		long							unregister_listener_msg(SmtListener *pListener);

	protected:
#ifdef SMT_THREAD_SAFE
		std::mutex						m_cslock;										//���̰߳�ȫ
#endif
		vSmtListenerPtrs				m_vListenerPtrs;
		SmtListener						*m_pActiveListener;
		mapMsgToPtr						m_mapMsgToListeners;

	private:
		SmtListenerManager(void);
		static SmtListenerManager*		m_pSingleton;
	};
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"platform_d.lib")
#       else
#          pragma comment(lib,"platform.lib")
#	    endif  
#endif

#endif //_SMT_LISTENER_MGR_H