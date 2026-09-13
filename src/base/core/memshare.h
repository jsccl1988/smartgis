/*
File:    smt_memshare.h

Desc:    SmtMemShare,���̼乲���ڴ�

Version: Version 1.0

Writter:  �´���

Date:    2012.11.3

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_MEMSHARE_H
#define _SMT_MEMSHARE_H

#include "base/core/core.h"

namespace base
{
	class CORE_EXPORT SmtMemShare
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

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_MEMSHARE_H
