/*
File:    sys_sysmanager.h

Desc:    SmtSysManager

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2026 The Mogu Authors.
All rights reserved.
*/
#ifndef _SYS_SYSMANAGER_H
#define _SYS_SYSMANAGER_H

#include "base/core/core.h"

#if defined(SYS_EXPORTS)
#define SYS_EXPORT __declspec(dllexport)
#else
#define SYS_EXPORT __declspec(dllimport)
#endif
#include "base/core/log.h"
#include "base/core/env_struct.h"
#include "base/core/msg_def.h"

using namespace base;

namespace sys
{
	class SYS_EXPORT SmtSysManager
	{
	private:
		SmtSysManager(void);

	public:
		virtual ~SmtSysManager(void);

	public:
		static SmtSysManager*		get_singleton_ptr(void);
		static void					destroy_instance(void);

	public:
		inline	SmtStyleConfig		get_sys_style_config(void)	const{return m_styleConfig;}	
		inline	void				set_sys_style_config(SmtStyleConfig &config){m_styleConfig = config;}	

		inline	SmtMapDocInfo		get_sys_map_doc_info(void)	const{return m_mapDocInfo;}	
		inline	void				set_sys_map_doc_info(SmtMapDocInfo &mapDocInfo){m_mapDocInfo = mapDocInfo;}	

		inline	SmtPrjInfo			get_sys_prj_info(void)	const{return m_prjInfo;}	
		inline	void				set_sys_prj_info(SmtPrjInfo &prjInfo){m_prjInfo = prjInfo;}

		inline	SmtSysPra			get_sys_pra(void) const{return m_sysPra;}
		inline	void				set_sys_pra(const SmtSysPra &sysPra){m_sysPra = sysPra;}

	private:
		SmtStyleConfig				m_styleConfig;
		SmtMapDocInfo				m_mapDocInfo;
		SmtPrjInfo					m_prjInfo;

		SmtSysPra					m_sysPra;
	
	private:
		static SmtSysManager       *m_pSingleton;
	};
}

#if !defined(SYS_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"platform_d.lib")
#       else
#          pragma comment(lib,"platform.lib")
#	    endif  
#endif

#endif //_SYS_SYSMANAGER_H