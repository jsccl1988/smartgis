/*
File:    smt_memshare.h

Desc:    SmtMemShare,进程间共享内存

Version: Version 1.0

Writter:  陈春亮

Date:    2012.11.3

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_MEMSHARE_H
#define _SMT_MEMSHARE_H

#include "smt_core.h"

namespace Smt_Core
{
	class SMT_EXPORT_CLASS SmtMemShare
	{
	public:
		SmtMemShare(const char * szMapFile,int nFileSize = 0,bool bServer = false);
		~SmtMemShare();

	public:
		const void *GetDataBuffer() const {return m_pDataBuffer;}

	protected:
		HANDLE		m_hFileMap;
		void		*m_pDataBuffer;
	};
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_MEMSHARE_H
