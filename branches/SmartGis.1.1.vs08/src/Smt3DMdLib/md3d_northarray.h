/*
File:   md3d_northarray.h

Desc:    SmtNorthArray,Ö¸±±Õë

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_NORTHARRAY_H
#define _MD3D_NORTHARRAY_H

#include "rd3d_3drenderer.h"
#include "rd3d_videobuffer.h"
#include "rd3d_3drenderdevice.h"
#include "rd3d_camera.h"
#include "bl3d_object.h"

using namespace Smt_3DBase;

namespace Smt_3DModel
{
	class SMT_EXPORT_CLASS SmtNorthArray:public Smt3DObject
	{
	public:
		SmtNorthArray(float initAngle,float fWinH,SmtPerspCamera* pCamera);
		virtual ~SmtNorthArray(void);

	public:
		long					Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName = "");
		long					Create(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed); 
		long					Render(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Destroy();

	public:
		void					SetPerspCamera(SmtPerspCamera* pCamera) {m_pCamera = pCamera;}
	private:
		void					DrawClock(LP3DRENDERDEVICE p3DRenderDevice);
		void					DrawArray(LP3DRENDERDEVICE p3DRenderDevice);

	private:
        SmtPerspCamera*			m_pCamera;
		float					m_fNorthPtAngle;
		float					m_fWinH;
		uint					m_nFontClock;
	
		SmtVertexBuffer			*m_pVBClockPan;
		SmtVertexBuffer			*m_pVBClockArray;
	};
}

#if     !defined(Export_Smt3DMdLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DMdLibD.lib")
#       else
#          pragma comment(lib,"Smt3DMdLib.lib")
#	    endif
#endif

#endif //_MD3D_NORTHARRAY_H