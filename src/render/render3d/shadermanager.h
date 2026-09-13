/*
File:    rd3d_shadermanager.h

Desc:     shaders������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _RD3D_SHADERMANAGER_H
#define _RD3D_SHADERMANAGER_H

#include "render/render3d/shader.h"
#include "base/core/filesys.h"
#include <map>

using namespace base;
using namespace render;

namespace render
{
	typedef vector<SmtShader*>						vShaderPtrs;
	typedef map<string,SmtShader*>					mapNameToShaderPtrs;
	typedef pair<string,SmtShader*>					pairNameToShaderPtr;

	class RENDER3D_EXPORT_CLASS SmtShaderManager
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

#if !defined(RENDER3D_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"render3dD.lib")
#       else
#          pragma comment(lib,"render3d.lib")
#	    endif
#endif


#endif //_RD3D_SHADERSMANAGER_H