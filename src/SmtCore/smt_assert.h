/*
File:    smt_assert.h

Desc:    SmartGis,Assert

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.12.9

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_ASSERT_H
#define _SMT_ASSERT_H

#include "smt_core.h"

#ifdef _DEBUG

bool SMT_EXPORT_API SmtAssert(bool bContent,char *szDesc,int nLine,char *szFile,bool *pBIgnoreAlways);

#define SMT_ASSERT(exp,desc) \
{\
	static bool bIgnoreAlways = false; \
	if (!bIgnoreAlways) \
	{\
		if (SmtAssert((int)(exp),desc,__LINE__,__FILE__,&bIgnoreAlways)) \
		{ \
		_asm { int 3} \
		}\
	}\
}
#else
#define SMT_ASSERT(exp,desc)
#endif // _DEBUG

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_ASSERT_H