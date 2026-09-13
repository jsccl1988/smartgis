/*
File:    t_iatoolmanager.h

Desc:    SmartGis IATool manager,�̰߳�ȫ

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _T_TOOL_MGR_H
#define _T_TOOL_MGR_H

#include "base/core/cslock.h"
#include "legacy_tool/t_iatool.h"
#include "legacy_tool/tool_export.h"

namespace tool
{
	class TOOL_EXPORT SmtIAToolManager
	{
	public:
		virtual~SmtIAToolManager(void);

	public:
		static SmtIAToolManager*		get_singleton_ptr(void);
		static void						DestoryInstance(void);

	public:
		long							notify(SmtIATool *pIATool,long lMsg,SmtListenerMsg &param);

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

#endif //_T_TOOL_MGR_H