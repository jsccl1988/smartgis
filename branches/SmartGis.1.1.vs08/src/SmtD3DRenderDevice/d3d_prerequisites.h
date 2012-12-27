/*
File:    d3d_prerequisites.h

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _D3D_PREREQUISITES_H 
#define _D3D_PREREQUISITES_H

#include "rd3d_base.h"

#include <d3dx9math.h >
#include <d3d9.h>

#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"d3dx10d.lib")
#pragma comment(lib,"d3dx9d.lib")
#pragma comment(lib,"dxerr.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"comctl32.lib")

#ifndef SMT_SAFE_RELEASE
#define SMT_SAFE_RELEASE( ptr ) { if (ptr!=NULL) { ptr->Release(); ptr = NULL; } }
#endif

#endif //_D3D_PREREQUISITES_H