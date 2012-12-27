/*
File:    sys_sysmanager.h

Desc:    SmtSysManager

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SYS_SYSMANAGER_H
#define _SYS_SYSMANAGER_H

#include "smt_core.h"
#include "smt_logmanager.h"
#include "smt_env_struct.h"
#include "smt_msg_def.h"

using namespace Smt_Core;

namespace Smt_Sys
{
	class SMT_EXPORT_CLASS SmtSysManager
	{
	private:
		SmtSysManager(void);

	public:
		virtual ~SmtSysManager(void);

	public:
		static SmtSysManager*		GetSingletonPtr(void);
		static void					DestoryInstance(void);

	public:
		inline	SmtStyleConfig		GetSysStyleConfig(void)	const{return m_styleConfig;}	
		inline	void				SetSysStyleConfig(SmtStyleConfig &config){m_styleConfig = config;}	

		inline	SmtMapDocInfo		GetSysMapDocInfo(void)	const{return m_mapDocInfo;}	
		inline	void				SetSysMapDocInfo(SmtMapDocInfo &mapDocInfo){m_mapDocInfo = mapDocInfo;}	

		inline	SmtPrjInfo			GetSysPrjInfo(void)	const{return m_prjInfo;}	
		inline	void				SetSysPrjInfo(SmtPrjInfo &prjInfo){m_prjInfo = prjInfo;}

		inline	SmtSysPra			GetSysPra(void) const{return m_sysPra;}
		inline	void				SetSysPra(const SmtSysPra &sysPra){m_sysPra = sysPra;}

	private:
		SmtStyleConfig				m_styleConfig;
		SmtMapDocInfo				m_mapDocInfo;
		SmtPrjInfo					m_prjInfo;

		SmtSysPra					m_sysPra;
	
	private:
		static SmtSysManager       *m_pSingleton;
	};
}

#if !defined(Export_SmtSysCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtSysCoreD.lib")
#       else
#          pragma comment(lib,"SmtSysCore.lib")
#	    endif  
#endif

#endif //_SYS_SYSMANAGER_H