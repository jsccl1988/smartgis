/*
File:   md3d_sphere.h

Desc:    SmtSphere,²âÊÔÄ£ÐÍ

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_SPHERE_H
#define _MD3D_SPHERE_H

#include "rd3d_3drenderer.h"
#include "rd3d_videobuffer.h"
#include "rd3d_3drenderdevice.h"
#include "bl3d_object.h"

using namespace Smt_3DBase;

namespace Smt_3DModel
{
	class SMT_EXPORT_CLASS SmtSphere :public Smt3DObject
	{
	public:

		SmtSphere(float radius, DWORD slices );
		virtual~SmtSphere();

	public:
		//
		long					Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName = "");
		long					Create(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed); 
		long					Render(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Destroy();

		inline void				SetXScale(float fScale){m_fXScale = fScale;}
		inline void				SetYScale(float fScale){m_fYScale = fScale;}
		inline void				SetZScale(float fScale){m_fZScale = fScale;}

		inline float			GetXScale(void){return m_fXScale;}
		inline float			GetYScale(void){return m_fYScale;}
		inline float			GetZScale(void){return m_fZScale;}

	public:
		bool					Select(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point);

	private:
		SmtVertexBuffer			*m_pVertexBuffer;
		float					m_fRadius;
		DWORD					m_dwSlices;

		float					m_fZScale;
		float					m_fXScale;
		float					m_fYScale;

	};
}

#if     !defined(Export_Smt3DMdLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DMdLibD.lib")
#       else
#          pragma comment(lib,"Smt3DMdLib.lib")
#	    endif
#endif

#endif //_MD3D_SPHERE_H
