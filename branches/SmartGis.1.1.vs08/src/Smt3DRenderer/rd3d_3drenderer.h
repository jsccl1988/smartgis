/*
File:    rd3d_3drenderer.h

Desc:    Smt3DRenderer,º”‘ÿµÿÕº‰÷»æ«˝∂Ø¿‡

Version: Version 1.0

Writter:  ≥¬¥∫¡¡

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _RD3D_RENDERER_H
#define _RD3D_RENDERER_H
#include "rd3d_3drenderdevice.h"

namespace Smt_3Drd
{
	class SMT_EXPORT_CLASS Smt3DRenderer
	{
	public:
		Smt3DRenderer(HINSTANCE hInst);
		~Smt3DRenderer(void);
		
		long					CreateDevice(const char *chAPI);
		LP3DRENDERDEVICE		GetDevice(void) { return m_pDevice; }
		HINSTANCE				GetModule(void) { return m_hDLL;    }
		void					Release(void);
		
	private:
		Smt3DRenderDevice		*m_pDevice;
		HINSTANCE				m_hInst;
		HMODULE					m_hDLL;
	};

    typedef Smt3DRenderer*		LPSMT3DRENDERER;
}

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif

#endif //_RD3D_RENDERER_H