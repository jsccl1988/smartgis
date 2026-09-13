/*
File:   md3d_northarray.h

Desc:    SmtNorthArray,ָ����

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_NORTHARRAY_H
#define _MD3D_NORTHARRAY_H

#include "render/render3d/3drenderer.h"
#include "render/render3d/videobuffer.h"
#include "render/render3d/3drenderdevice.h"
#include "render/render3d/camera.h"
#include "render/scene3d/bl3d_object.h"

#if !defined(MODEL3D_EXPORT_DEFINED)
#define MODEL3D_EXPORT_DEFINED
#if defined(MODEL3D_EXPORTS)
#define MODEL3D_EXPORT_API __declspec(dllexport)
#define MODEL3D_EXPORT_CLASS __declspec(dllexport)
#else
#define MODEL3D_EXPORT_API __declspec(dllimport)
#define MODEL3D_EXPORT_CLASS __declspec(dllimport)
#endif
#endif

using namespace render;

namespace render
{
	class MODEL3D_EXPORT_CLASS SmtNorthArray:public Smt3DObject
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

#if     !defined(MODEL3D_EXPORTS)
#if     defined(_DEBUG)
#          pragma comment(lib,"model3dD.lib")
#       else
#          pragma comment(lib,"model3d.lib")
#	    endif
#endif

#endif //_MD3D_NORTHARRAY_H