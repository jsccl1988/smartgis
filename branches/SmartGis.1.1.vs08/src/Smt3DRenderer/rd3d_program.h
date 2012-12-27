/*
File:    rd3d_program.h

Desc:    SmtProgram

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_PROGRAM_H
#define _RD3D_PROGRAM_H

#include "rd3d_3drenderdefs.h"
#include "rd3d_base.h"
#include "rd3d_shader.h"

namespace Smt_3Drd
{
	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class SMT_EXPORT_CLASS SmtProgram
	{	
	public:
		SmtProgram(LP3DRENDERDEVICE p3DRenderDevice, uint handle,string strName);
		virtual ~SmtProgram();

	public:
		inline uint				GetHandle() {return m_unHandle;}
		const char				*GetProgramName(void) {return m_strName.c_str();}

	public:
		long					Use();
		long					Unuse();

		virtual long			SetVertexShader(SmtShader *shader);
		virtual long			SetPixelShader(SmtShader *shader);

		virtual long			Link(ShaderCompilationFlag flags = SCF_LOG_ERRORS);
		virtual long			IsLinked();
		virtual char			*GetLinkLog();

		virtual long			SetVector(string param, const Vector4 &value);
		virtual long			SetVector(string param, const Vector3 &value);
		virtual long			SetVector(string param, const Vector2 &value);
		virtual long			SetFloat(string param, float value);
		virtual long			SetInt(string param, int value);
		virtual long			GetFloat(string param, float *value);
		virtual long			SetTexture(string param, int texture);

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
		uint					m_unHandle;
		string					m_strName;	
	};
}

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif

#endif //_RD3D_PROGRAM_H
