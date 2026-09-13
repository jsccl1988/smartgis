/*
File:    smt_dynlib.h

Desc:    SmartGis ��̬����

Version: Version 1.0

Writter:  �´���

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_DYNLIB_H
#define _SMT_DYNLIB_H

#include "base/core/core.h"

namespace base
{
	const string g_strDynLibLog = "SmtDynLib";
	class CORE_EXPORT SmtDynLib
	{
	public:
		SmtDynLib(const char * name,const char * path);
		virtual ~SmtDynLib(void);

	public:
        inline const char * get_name(void) const{return m_szName;}
		inline const char *	get_path(void) const{return m_szPath;}

		virtual bool		load(void);
		virtual void		unload(void);

	protected:
		char				m_szName[MAX_NAME_LENGTH];
		char				m_szPath[MAX_FILE_PATH];
		HMODULE				m_hDLL;
	};

	typedef vector<SmtDynLib*> vSmtDynLibPtrs;
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"coreD.lib")
#       else
#          pragma comment(lib,"core.lib")
#	    endif  
#endif

#endif //_SMT_DYNLIB_H