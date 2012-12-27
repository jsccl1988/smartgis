/*
File:    rd3d_programmanager.h

Desc:     Programπ‹¿Ì¿‡

Version: Version 1.0

Writter:  ≥¬¥∫¡¡

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _RD3D_PROGRAMMANAGER_H
#define _RD3D_PROGRAMMANAGER_H

#include "rd3d_program.h"
#include <map>

using namespace Smt_Core;
using namespace Smt_3Drd;

namespace Smt_3Drd
{
	typedef vector<SmtProgram*>						vProgramPtrs;
	typedef map<string,SmtProgram*>					mapNameToProgramPtrs;
	typedef pair<string,SmtProgram*>				pairNameToProgramPtr;

	class SMT_EXPORT_CLASS SmtProgramManager
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

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif


#endif //_RD3D_PROGRAMMANAGER_H