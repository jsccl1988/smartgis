/*
File:    smt_listener.h

Desc:    SmartGis listener

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_LISTENER_H
#define _SMT_LISTENER_H

#include "smt_core.h"
#include "smt_bas_struct.h"
#include "smt_msg_def.h"

namespace Smt_Core
{
	class SMT_EXPORT_CLASS SmtListener
	{
	public:
		SmtListener(void) ;
		virtual ~SmtListener(void) ;

	public:
		virtual int						Register(void);
		virtual int						RegisterMsg(void);

		virtual int						UnRegister(void);
		virtual int						UnRegisterMsg(void);

		virtual int						SetActive();
		
		virtual int						Notify(long lMsg,SmtListenerMsg &param) = 0;
	
	public:
		const char*						GetName() const;
		void							SetName(const char*szName) ;

		bool							AppendFuncItems(const char *szFunc,long lFuncMsg,long lStyle = FIM_2DVIEW|FIM_3DVIEW);
		vSmtFuncItems					GetFuncItems(SmtFuncItemStyle style) ;

		vSmtMsgs						GetMsgs(void)  { return m_vMsgs;}
		

	protected:
		bool							AppendFuncItems(const char *szFunc,long lFuncMsg,vSmtFuncItems &vFuncItems);
		bool							AppendMsg(long lFuncMsg);

	protected:
		char							m_szListenerName[SMT_GROUP_NAME_LENGTH];

		vSmt2DViewFuncItems				m_v2DViewFuncItems;
		vSmt3DViewFuncItems				m_v3DViewFuncItems;
		vSmt3DExViewFuncItems			m_v3DExViewFuncItems;
		vSmtMapDocCatalogFuncItems		m_vMDCatalogFuncItems;
		vSmt2DToolBarFuncItems			m_v2DToolBarFuncItems;
		vSmt3DToolBarFuncItems			m_v3DToolBarFuncItems;
		vSmt2DMenuFuncItems				m_v2DMMenuFuncItems;
		vSmt3DMenuFuncItems				m_v3DMMenuFuncItems;
		vSmtAuxModuleBoxFuncItems		m_vAMBoxFuncItems;
		vSmtAuxModuleTreeFuncItems		m_vAMTreeFuncItems;	
		vSmtMsgs						m_vMsgs;
	};

	typedef vector<SmtListener*>		vSmtListenerPtrs;
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_LISTENER_H