/*
File:   md3d_sphere.h

Desc:    SmtSphere,����ģ��

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_SPHERE_H
#define _MD3D_SPHERE_H

#include "legacy/render/render3d/3drenderer.h"
#include "legacy/render/render3d/videobuffer.h"
#include "legacy/render/render3d/3drenderdevice.h"
#include "legacy/render/scene3d/bl3d_object.h"

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
	class MODEL3D_EXPORT_CLASS SmtSphere :public Smt3DObject
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

#if     !defined(MODEL3D_EXPORTS)
#if     defined(_DEBUG)
#          pragma comment(lib,"legacy_render_d.lib")
#       else
#          pragma comment(lib,"legacy_render.lib")
#	    endif
#endif

#endif //_MD3D_SPHERE_H
