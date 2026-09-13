/*
File:    smt_assert.h

Desc:    SmartGis,Assert

Version: Version 1.0

Writter:  �´���

Date:    2011.12.9

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_ASSERT_H
#define _SMT_ASSERT_H

#include "base/core/core.h"

#ifdef _DEBUG

bool CORE_EXPORT smt_assert(bool bContent,char *szDesc,int nLine,char *szFile,bool *pBIgnoreAlways);

#define SMT_ASSERT(exp,desc) \
{\
	static bool bIgnoreAlways = false; \
	if (!bIgnoreAlways) \
	{\
		if (smt_assert((int)(exp),desc,__LINE__,__FILE__,&bIgnoreAlways)) \
		{ \
		_asm { int 3} \
		}\
	}\
}
#else
#define SMT_ASSERT(exp,desc)
#endif // _DEBUG

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_ASSERT_H