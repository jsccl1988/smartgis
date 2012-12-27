/*
File:    smt_dynlib.h

Desc:    SmartGis ¶¯Ì¬¿âÀà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_DYNLIB_H
#define _SMT_DYNLIB_H

#include "smt_core.h"

namespace Smt_Core
{
	const string g_strDynLibLog = "SmtDynLib";
	class SMT_EXPORT_CLASS SmtDynLib
	{
	public:
		SmtDynLib(const char * name,const char * path);
		virtual ~SmtDynLib(void);

	public:
        inline const char * GetName(void) const{return m_szName;}
		inline const char *	GetPath(void) const{return m_szPath;}

		virtual bool		Load(void);
		virtual void		UnLoad(void);

	protected:
		char				m_szName[MAX_NAME_LENGTH];
		char				m_szPath[MAX_FILE_PATH];
		HMODULE				m_hDLL;
	};

	typedef vector<SmtDynLib*> vSmtDynLibPtrs;
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_DYNLIB_H