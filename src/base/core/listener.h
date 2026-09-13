/*
File:    smt_listener.h

Desc:    SmartGis listener

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_LISTENER_H
#define _SMT_LISTENER_H

#include "base/core/core.h"
#include "base/core/bas_struct.h"
#include "base/core/msg_def.h"

namespace base
{
	class CORE_EXPORT SmtListener
	{
	public:
		SmtListener(void) ;
		virtual ~SmtListener(void) ;

	public:
		virtual int						register_(void);
		virtual int						register_msg(void);

		virtual int						unregister(void);
		virtual int						unregister_msg(void);

		virtual int						set_active();
		
		virtual int						notify(long lMsg,SmtListenerMsg &param) = 0;
	
	public:
		const char*						get_name() const;
		void							set_name(const char*szName) ;

		bool							append_func_items(const char *szFunc,long lFuncMsg,long lStyle = FIM_2DVIEW|FIM_3DVIEW);
		vSmtFuncItems					get_func_items(SmtFuncItemStyle style) ;

		vSmtMsgs						get_msgs(void)  { return m_vMsgs;}
		

	protected:
		bool							append_func_items(const char *szFunc,long lFuncMsg,vSmtFuncItems &vFuncItems);
		bool							append_msg(long lFuncMsg);

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

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_LISTENER_H