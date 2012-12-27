/*
File:   md3d_water.h

Desc:    SmtNorthArray,Ë®

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_WATER_H
#define _MD3D_WATER_H

#include "rd3d_3drenderer.h"
#include "rd3d_videobuffer.h"
#include "rd3d_3drenderdevice.h"
#include "bl3d_object.h"

using namespace Smt_3DBase;

const		double					CST_DBF_MAX_DELTA = 0.01;
const		double					CST_DBF_ANIMATION_SPEED = 1.;

const		int						CST_INT_GRID_WIDTH = 128;
const		int						CST_INT_GRID_HEIGHT = 128;
const		int						CST_INT_GRID_SIZE = CST_INT_GRID_WIDTH*CST_INT_GRID_HEIGHT;


namespace Smt_3DModel
{
	struct WVertex
	{
		float x,y,z;
		float nx,ny,nz;
		float u,v;
		float r,g,b;
	};

	class SMT_EXPORT_CLASS	SmtWater :public Smt3DObject
	{
	public:
		SmtWater();
		virtual~SmtWater();

	public:
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

	protected:
		void					Compute(float dt = .1);
		void					AdjustGrid(void);	


		void					UpdateVertex(void);
		void					UpdateNormal(void);
		void					UpdateTexcoord(void);

		void					UpdateVB(void);

	private:
		SmtVertexBuffer*		m_pVertexBuffer;
		Vector3					m_vCenter;
		float					m_fWidth;
		float					m_fTexOffset;

		float					m_fZScale;
		float					m_fXScale;
		float					m_fYScale;

	private:
		double					p[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT];
		double					vx[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT], vy[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT];
		double					ax[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT], ay[CST_INT_GRID_WIDTH][CST_INT_GRID_HEIGHT];
		WVertex					wvertex[CST_INT_GRID_SIZE];
	};
}

#if     !defined(Export_Smt3DMdLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DMdLibD.lib")
#       else
#          pragma comment(lib,"Smt3DMdLib.lib")
#	    endif
#endif

#endif //_MD3D_WATER_H