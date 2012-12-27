/*
File:    rd_renderdevice.h

Desc:    SmtRenderDevice,地图渲染驱动基类
			1.多渲染层：Quick		2		Direct 直接在DC上绘制，但不能驻留，调用刷新之后则消失
						Dynamic		1
						Map			0

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD_RENDERDEVICE_H
#define _RD_RENDERDEVICE_H

#include "smt_core.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "bl_bas_struct.h"

using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Base;
using namespace Smt_Geo;

namespace Smt_Rd
{
	enum eRDBufferLayer
	{
		MRD_BL_MAP,
		MRD_BL_DYNAMIC,
		MRD_BL_QUICK,
		MRD_BL_DIRECT
	};

	class SmtRenderDevice
	{
	public:
		SmtRenderDevice(HINSTANCE hInst):m_rBaseApi(RD_GDI)
			,m_hInst(hInst)
			,m_strLogName("")
			,m_hWnd(NULL)
			,m_nMapMode(MM_TEXT)
			,m_fblc(1.)
		{ 
			 ;
		}

		virtual ~SmtRenderDevice(void){}

	public:
		virtual int             Init(HWND hWnd,const char * logname) = 0;
		virtual int				Destroy(void) = 0;
		virtual int             Release(void) = 0;

		virtual int				Resize(int orgx,int orgy,int cx,int cy) = 0;

		void                    SetViewport(const Viewport &viewport) {m_Viewport = viewport;}
		Viewport				GetViewport(void) const {return m_Viewport;}

		void                    SetWindowport(const Windowport &windowport) {m_Windowport = windowport;}
		Windowport				GetWindowport(void) const {return m_Windowport ;}

		void					SetCurDrawingOrg(const lPoint &ptPos){m_curDrawingOrg = ptPos;}
		lPoint					GetCurDrawingOrg(void) const { return m_curDrawingOrg;}

		void                    SetMapMode(int nMode) { m_nMapMode = nMode; }
		int                     GetMapMode(void) const { return m_nMapMode;}

		void					SetRenderPra(const Smt2DRenderPra	&rdPra) {m_rdPra = rdPra;}
		Smt2DRenderPra			GetRenderPra(void) const{return m_rdPra;}

		inline double			GetBlc(void) const { return m_fblc;}

	public:
		virtual	int				Lock() = 0;
		virtual	int				Unlock() = 0;

		virtual int             Refresh(void) = 0;
		virtual int             Refresh(const SmtMap *pMap,fRect rect) = 0;
		virtual int             RefreshDirectly(const SmtMap *pMap,lRect rect,bool bRealTime = false) = 0;

		virtual int             ZoomMove(const SmtMap *pMap,fPoint dbfPointOffset,bool bRealTime = false) = 0;
		virtual int             ZoomScale(const SmtMap *pMap,lPoint orgPoint,float fscale,bool bRealTime = false) = 0;
		virtual int             ZoomToRect(const SmtMap *pMap,fRect rect,bool bRealTime = false) = 0;

		virtual	int				Timer() = 0;

	public:
		virtual int				LPToDP(float x,float y,long &X,long &Y) const = 0;
		virtual int				DPToLP(LONG X,LONG Y,float &x,float &y) const = 0;
		virtual int				LRectToDRect(const fRect &frect,lRect &lrect) const = 0;
		virtual int             DRectToLRect(const lRect &lrect,fRect &frect) const = 0;

	public:
		virtual	int             BeginRender(eRDBufferLayer eMRDBufLyr,bool bClear = false,const SmtStyle*pStyle = NULL,int op = R2_COPYPEN) = 0;
		virtual int             EndRender(eRDBufferLayer eMRDBufLyr) = 0;

	public:
		virtual int             RenderMap(void) = 0;
		
		virtual int             RenderMap(const SmtMap *pMap,int op = R2_COPYPEN) = 0;						//1-16 R2_BLACK-R2_WHITE
		virtual int             RenderLayer(const SmtLayer *pLayer,int op = R2_COPYPEN) = 0;				//1-16 R2_BLACK-R2_WHITE
		virtual int             RenderLayer(const SmtVectorLayer *pLayer,int op = R2_COPYPEN) = 0;			//1-16 R2_BLACK-R2_WHITE
		virtual int             RenderLayer(const SmtRasterLayer *pLayer,int op = R2_COPYPEN) = 0;			//1-16 R2_BLACK-R2_WHITE
		virtual int             RenderLayer(const SmtTileLayer *pLayer,int op = R2_COPYPEN) = 0;			//1-16 R2_BLACK-R2_WHITE
		virtual int             RenderFeature(const SmtFeature *pFeature,int op = R2_COPYPEN) = 0;			//1-16 R2_BLACK-R2_WHITE
		virtual int             RenderGeometry(const SmtGeometry *pGeom,const SmtStyle*pStyle,int op = R2_COPYPEN) = 0;

		virtual int             DrawMultiLineString(const SmtMultiLineString *pMultiLinestring) = 0;
		virtual int             DrawMultiPoint(const SmtStyle*pStyle,const SmtMultiPoint *pMultiPoint) = 0;
		virtual int             DrawMultiPolygon(const SmtMultiPolygon *pMultiPolygon) = 0;

		virtual int             DrawPoint(const SmtStyle*pStyle,const SmtPoint *pPoint) = 0;
		virtual int             DrawAnno(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const SmtPoint *pPoint) = 0;
		virtual int             DrawSymbol(HICON hIcon,long lHeight,long lWhidth,const SmtPoint *pPoint) = 0;

		virtual int             DrawLineString(const SmtLineString *pLinestring) = 0;
		virtual int             DrawLineSpline(const SmtSpline *pSpline) = 0;
		virtual int             DrawLinearRing(const SmtLinearRing *pLinearRing) = 0;
		virtual int             DrawPloygon(const SmtPolygon *pPloygon) = 0;

		virtual int             DrawTin(const SmtTin *pTin) = 0;
		virtual int             DrawGrid(const SmtGrid *pGrid) = 0;
		virtual int             DrawArc(const SmtArc *pArc) = 0;
		virtual int             DrawFan(const SmtFan *pFan) = 0;

	public:
		virtual int             DrawEllipse(float left,float top,float right,float bottom,bool bDP = false) = 0;
		virtual int             DrawRect(const fRect &rect,bool bDP = false) = 0;
		virtual int             DrawLine(fPoint *pfPoints,int nCount,bool bDP = false) = 0;
		virtual int             DrawLine(const fPoint &ptA,const fPoint &ptB,bool bDP = false) = 0;
		virtual int             DrawText(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const fPoint &point,bool bDP = false) = 0;
		virtual int             DrawImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType,eRDBufferLayer eMRDBufLyr = MRD_BL_MAP) = 0;
		virtual int             StrethImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType,eRDBufferLayer eMRDBufLyr = MRD_BL_MAP) = 0;

	public:
		virtual	int				SaveImage(const char * szFilePath,eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,bool bBgTransparent = false ) = 0;
		virtual int				Save2ImageBuf(char *&szImageBuf,long &lImageBufSize,long lCodeType,eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,bool bBgTransparent = false) = 0;
		virtual int				FreeImageBuf(char *&szImageBuf) = 0;

	protected:
		HINSTANCE               m_hInst;
		RenderBaseApi			m_rBaseApi;
		string                  m_strLogName;

		Viewport				m_Viewport;
		Windowport				m_Windowport;
		float					m_fblc;

		HWND                    m_hWnd;
		int                     m_nMapMode;
		Smt2DRenderPra			m_rdPra;

		lPoint					m_curDrawingOrg;
	};

	typedef SmtRenderDevice* LPRENDERDEVICE;

#ifdef __cplusplus
	extern "C" {
#endif
		int SMT_EXPORT_DLL CreateRenderDevice(HINSTANCE hInst,LPRENDERDEVICE &pMrdDevice);
		int SMT_EXPORT_DLL DestroyRenderDevice(LPRENDERDEVICE &pMrdDevice);

		typedef HRESULT (*_CreateRenderDevice)(HINSTANCE hInst,LPRENDERDEVICE &pMrdDevice);
		typedef HRESULT (*_DestroyRenderDevice)(LPRENDERDEVICE &pMrdDevice);

#ifdef __cplusplus
	}
#endif
}

#endif //_RD_RENDERDEVICE_H