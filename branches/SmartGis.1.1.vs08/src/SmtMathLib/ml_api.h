/*
File:    2dmathlib.h

Desc:    ¶þÎ¬ÊýÑ§¿â

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_MATH_LIB_API_H
#define _SMT_MATH_LIB_API_H

#include "ml_mathlib.h"

#if     !defined(Export_SmtMathLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"SmtMathLibD.lib")
#       else
#          pragma comment(lib,"SmtMathLib.lib")
#	    endif
#endif

#endif //_SMT_MATH_LIB_API_H