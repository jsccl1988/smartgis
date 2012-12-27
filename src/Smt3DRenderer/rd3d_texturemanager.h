/*
File:    rd3d_texturemanager.h

Desc:     三维 纹理管理类

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _RD3D_TEXTUREMANAGER_H
#define _RD3D_TEXTUREMANAGER_H

#include "rd3d_texture.h"
#include "smt_filesys.h"
#include <map>

using namespace Smt_Core;
using namespace Smt_3Drd;

namespace Smt_3Drd
{
	typedef vector<SmtTexture*>						vTexturePtrs;
	typedef map<string,SmtTexture*>					mapNameToTexturePtrs;
	typedef pair<string,SmtTexture*>				pairNameToTexturePtr;

	class SMT_EXPORT_CLASS SmtTextureManager
	{
	public:
		SmtTextureManager(void);
		virtual ~SmtTextureManager(void);

	public:
	//	static SmtTexture*				CreateTexture(LP3DRENDERDEVICE pRenderDevice,SmtFileInfo &info,const string strName);

	public:
		long							AddTexture(SmtTexture*	pTexture);
		SmtTexture*						GetTexture(const char * szName);
		long							DestroyTexture(const char * szName);
		long							DestroyAllTexture(void);

		void							GetAllTextureName(vector<string> &vStrAllTextureName);

	private:
        mapNameToTexturePtrs			m_mapNameToTexturePtrs;
	};
}

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif


#endif //_RD3D_TEXTUREMANAGER_H