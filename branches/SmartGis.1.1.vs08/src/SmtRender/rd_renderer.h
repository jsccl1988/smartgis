/*
File:    rd_renderer.h

Desc:    SmtRenderer,º”‘ÿµÿÕº‰÷»æ«˝∂Ø¿‡

Version: Version 1.0

Writter:  ≥¬¥∫¡¡

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD_RENDERER_H
#define _RD_RENDERER_H

#include "smt_core.h"
#include "rd_renderdevice.h"

namespace Smt_Rd
{
	class SMT_EXPORT_CLASS SmtRenderer
	{
	public:
		SmtRenderer(HINSTANCE hInst);
		~SmtRenderer(void);

		int                  CreateDevice(const char *chAPI);
		LPRENDERDEVICE		 GetDevice(void) { return m_pDevice; }
		void                 Release(void);

	private:
		LPRENDERDEVICE		 m_pDevice;
		HINSTANCE            m_hInst;
		HMODULE				 m_hDLL;
	};

	typedef SmtRenderer* LPRENDERER;
}

#if !defined(Export_SmtRenderer)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtRendererD.lib")
#       else
#          pragma comment(lib,"SmtRenderer.lib")
#	    endif  
#endif

#endif //_RD_RENDERER_H