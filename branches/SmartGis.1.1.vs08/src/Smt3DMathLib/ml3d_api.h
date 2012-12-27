/*
File:    3dmathlib.h

Desc:    ÈýÎ¬ÊýÑ§¿â

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_3D_MATH_LIB_API_H
#define _SMT_3D_MATH_LIB_API_H

#include "ml3d_mathlib.h"

using namespace Smt_3DMath;

Vector4 SMT_EXPORT_API GalcTriangleNormal( Vector4 &v1 ,Vector4 &v2, Vector4 &v3 );

#if     !defined(Export_Smt3DMathLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DMathLibD.lib")
#       else
#          pragma comment(lib,"Smt3DMathLib.lib")
#	    endif
#endif

#endif //_SMT_3D_MATH_LIB_API_H