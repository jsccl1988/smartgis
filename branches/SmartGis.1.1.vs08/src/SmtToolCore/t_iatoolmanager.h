/*
File:    t_iatoolmanager.h

Desc:    SmartGis IATool manager,线程安全

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _T_TOOL_MGR_H
#define _T_TOOL_MGR_H

#include "smt_core.h"
#include "t_iatool.h"
#include "smt_cslock.h"

namespace Smt_IATool
{
	class SMT_EXPORT_CLASS SmtIAToolManager
	{
	public:
		virtual~SmtIAToolManager(void);

	public:
		static SmtIAToolManager*		GetSingletonPtr(void);
		static void						DestoryInstance(void);

	public:
		long							Notify(SmtIATool *pIATool,long lMsg,SmtListenerMsg &param);

		void							SetActiveIATool(SmtIATool*pIATool) {m_pActiveIATool = pIATool;}
		SmtIATool*						GetActiveIATool(void) {return m_pActiveIATool;}
		const SmtIATool*				GetActiveIATool(void) const{return m_pActiveIATool;}

		long							RegisterIATool(SmtIATool *pIATool);
		long							RemoveIATool(SmtIATool*pIATool);
		long							RemoveAllIATool(void);

		SmtIATool*						GetIATool(int index);
		const SmtIATool*				GetIATool(int index) const;
		int								GetIAToolCount(void) const{return m_vIAToolPtrs.size();}
		
		long							RegisterIAToolMsg(SmtIATool *pIATool);
		long							UnRegisterIAToolMsg(SmtIATool *pIATool);

	protected:

#ifdef SMT_THREAD_SAFE
		SmtCSLock						m_cslock;
#endif

		vSmtIAToolPtrs					m_vIAToolPtrs;
		SmtIATool						*m_pActiveIATool;
		mapMsgToPtr						m_mapMsgToIATools;

	private:
		SmtIAToolManager(void);
		static SmtIAToolManager*		m_pSingleton;
	};
}

#if !defined(Export_SmtToolCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtToolCoreD.lib")
#       else
#          pragma comment(lib,"SmtToolCore.lib")
#	    endif  
#endif

#endif //_T_TOOL_MGR_H