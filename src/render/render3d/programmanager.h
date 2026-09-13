/*
File:    rd3d_programmanager.h

Desc:     Program������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _RD3D_PROGRAMMANAGER_H
#define _RD3D_PROGRAMMANAGER_H

#include "render/render3d/program.h"
#include <map>

using namespace base;
using namespace render;

namespace render
{
	typedef vector<SmtProgram*>						vProgramPtrs;
	typedef map<string,SmtProgram*>					mapNameToProgramPtrs;
	typedef pair<string,SmtProgram*>				pairNameToProgramPtr;

	class RENDER3D_EXPORT_CLASS SmtProgramManager
	{
	public:
		SmtProgramManager(void);
		virtual ~SmtProgramManager(void);

	public:
		long							AddProgram(SmtProgram* pProgram);
		SmtProgram*						GetProgram(const char * szName);
		void							DestroyProgram(const char * szName);
		void							DestroyAllProgram(void);

		void							GetAllProgramName(vector<string> &vStrAllProgramName);
	private:
        mapNameToProgramPtrs			m_mapNameToProgramPtrs;
	};
}

#if !defined(RENDER3D_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"render3dD.lib")
#       else
#          pragma comment(lib,"render3d.lib")
#	    endif
#endif


#endif //_RD3D_PROGRAMMANAGER_H