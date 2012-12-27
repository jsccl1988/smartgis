/*
File:   md3d_cube.h

Desc:    SmtCube,²âÊÔÄ£ÐÍ

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _MD3D_CUBE_H
#define _MD3D_CUBE_H

#include "rd3d_3drenderer.h"
#include "rd3d_videobuffer.h"
#include "rd3d_3drenderdevice.h"
#include "bl3d_object.h"

using namespace Smt_3DBase;

namespace Smt_3DModel
{
	class SMT_EXPORT_CLASS SmtCube :public Smt3DObject
	{
	public:

		SmtCube(LP3DRENDERDEVICE pRenderDevice,Vector3 vCenter, float width);
		virtual~SmtCube();

	public:
		long					Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName = "");
		long					Create(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Render(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed); 
		long					Destroy();

	private:
		SmtVertexBuffer*		m_pVertexBuffer;
		Vector3					m_vCenter;
		float					m_fWidth;
	};
}

#if     !defined(Export_Smt3DMdLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DMdLibD.lib")
#       else
#          pragma comment(lib,"Smt3DMdLib.lib")
#	    endif
#endif


#endif //_MD3D_CUBE_H