/*
File:    rd3d_shadermanager.h

Desc:     shadersπ‹¿Ì¿‡

Version: Version 1.0

Writter:  ≥¬¥∫¡¡

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _RD3D_SHADERMANAGER_H
#define _RD3D_SHADERMANAGER_H

#include "rd3d_shader.h"
#include "smt_filesys.h"
#include <map>

using namespace Smt_Core;
using namespace Smt_3Drd;

namespace Smt_3Drd
{
	typedef vector<SmtShader*>						vShaderPtrs;
	typedef map<string,SmtShader*>					mapNameToShaderPtrs;
	typedef pair<string,SmtShader*>					pairNameToShaderPtr;

	class SMT_EXPORT_CLASS SmtShaderManager
	{
	public:
		SmtShaderManager(void);
		virtual ~SmtShaderManager(void);

	public:
		long							AddShader(SmtShader* pShader);
		SmtShader*						GetShader(const char * szName);
		void							DestroyShader(const char * szName);
		void							DestroyAllShader(void);

		void							GetAllShaderName(vector<string> &vStrAllShaderName);

	private:
        mapNameToShaderPtrs				m_mapNameToShaderPtrs;

	};
}

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif


#endif //_RD3D_SHADERSMANAGER_H