/*
File:    rd3d_shader.h

Desc:    SmtShader

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_SHADER_H
#define _RD3D_SHADER_H

#include "rd3d_3drenderdefs.h"

namespace Smt_3Drd
{
	/**
	Defines additional actions for shader compilation and program linking process.
	*/
	enum ShaderCompilationFlag
	{
		SCF_NOTHING = 0,
		SCF_CHECK_ERRORS = 1,
		SCF_LOG_ERRORS = SCF_CHECK_ERRORS | 2,
	};

	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class SMT_EXPORT_CLASS SmtShader
	{	
	public:
		SmtShader(LP3DRENDERDEVICE p3DRenderDevice, uint handle,string strName);
		virtual ~SmtShader();

	public:
		inline uint				GetHandle() {return m_unHandle;}
		const char				*GetShaderName(void) {return m_strName.c_str();}

	public:
		virtual long			Load(string fileName, bool needToCompile = false,ShaderCompilationFlag flags = SCF_NOTHING);
		virtual long			Compile(ShaderCompilationFlag flags = SCF_LOG_ERRORS);
		virtual long			IsCompiled();
		virtual char*			GetCompileLog();

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

#endif //_RD3D_SHADER_H