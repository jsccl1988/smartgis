/*
File:    gdi_renderthread.h

Desc:    SmtGdiMrdThread,GDI 地图渲染线程

Version: Version 1.0

Writter:  陈春亮

Date:    2011.11.14

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GDI_RENDERTHREAD_H
#define _GDI_RENDERTHREAD_H

#include "smt_thread.h"
#include "smt_cslock.h"
#include "bl_bas_struct.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "gdi_renderbuf.h"
#include "gdi_bufpool.h"

using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Base;
using namespace Smt_Geo;

namespace Smt_Rd
{
	//class  SmtGdiRenderDevice;
	struct SmtRenderContex
	{
		Viewport		 viewport;				//渲染视口
		Windowport		 windowport;			//渲染窗口
		float            fblc;					//比值
		SmtMap			 *pMap;					//渲染地图
		int				 orgx,orgy;				//渲染范围,左下角坐标
		int				 width,height;			//渲染范围,宽度、高度
		int				 op;					//渲染模式
		
		SmtRenderContex():fblc(1)
			,pMap(NULL),orgx(0),orgy(0),width(0),height(0),op(0)
			
		{

		}

		SmtRenderContex(Viewport vp,Windowport wp,float _fblc, const SmtMap *_pMap,int _x,int _y,int _w,int _h,int _op = R2_COPYPEN):viewport(vp)
			,windowport(wp),fblc(_fblc)
			,pMap(const_cast<SmtMap *>(_pMap))
			,orgx(_x),orgy(_y),width(_w),height(_h),op(_op)
			
		{

		}
	};

	class SmtGdiRenderThread:public SmtThread
	{
	public:
		SmtGdiRenderThread(HINSTANCE hInst,Viewport	 &virViewport1,Viewport	 &virViewport2);
		virtual ~SmtGdiRenderThread(void);

	public:
		virtual void	Run(void *pParam/*SmtRenderContex *pSmtRC*/);			//线程运行函数
	
	public:
		int             Init(HWND hWnd,const char * logname);
		int				Resize(int orgx,int orgy,int cx,int cy);
		int				ShareBuf(SmtRenderBuf &smtSharedBuf);
		int				SetRenderPra(const Smt2DRenderPra	&rdPra) {m_rdPra = rdPra;return SMT_ERR_NONE;}
		Smt2DRenderPra	GetRenderPra(void) {return m_rdPra;}

	public:
		bool			IsRendering(void) const {return m_bRendering;}
		
		int				SetRenderContex(SmtRenderContex &smtRC) { if(!IsRendering())m_smtRC = smtRC;return SMT_ERR_NONE;}
		int				GetRenderContex(SmtRenderContex &smtRC) const { smtRC =  m_smtRC;}

		int				ClearBuf(int x,int y,int w,int h,COLORREF clr = RGB(255,255,255)) {if(!IsRendering())m_smtRenderBuf.ClearBuf(x,y,w,h,clr);return SMT_ERR_NONE;}
	
		int				SwapBuf(int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int op = SRCCOPY);
		int				SwapBuf(int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int srcW,int srcH,eSwapType type = eSwapType::BLT_TRANSPARENT,int op = SRCCOPY,COLORREF clr = RGB(255,255,255));

		int				SwapBuf(SmtRenderBuf &rbTarget,int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int op = SRCCOPY);
		int				SwapBuf(SmtRenderBuf &rbTarget,int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int srcW,int srcH,eSwapType type = eSwapType::BLT_TRANSPARENT,int op = SRCCOPY,COLORREF clr = RGB(255,255,255));

	public:
		int				LPToDP(float x,float y,long &X,long &Y) const  ;
		int				DPToLP(LONG X,LONG Y,float &x,float &y) const  ;
		int             LRectToDRect(const fRect &frect,lRect &lrect) const  ;
		int             DRectToLRect(const lRect &lrect,fRect &frect) const  ;

		int             RenderMap( const  SmtMap *pMap,int x,int y,int w,int h,int op = R2_COPYPEN);

	public:
		int             RenderLayer(const SmtLayer *pLayer,int op = R2_COPYPEN);				//1-16 R2_BLACK-R2_WHITE
		int             RenderLayer(const SmtVectorLayer *pLayer,int op = R2_COPYPEN);			//1-16 R2_BLACK-R2_WHITE
		int             RenderLayer(const SmtRasterLayer *pLayer,int op = R2_COPYPEN);			//1-16 R2_BLACK-R2_WHITE
		int             RenderLayer(const SmtTileLayer *pLayer,int op = R2_COPYPEN);			//1-16 R2_BLACK-R2_WHITE

		int             RenderFeature( const  SmtFeature *pFeature,int op = R2_COPYPEN);
		int             RenderGeometry( const  SmtGeometry *pGeom, const  SmtStyle*pStyle,int op = R2_COPYPEN);

	public:
		int             PrepareForDrawing( const  SmtStyle*pStyle,int nDrawMode = R2_COPYPEN);
		int             EndDrawing(void);

		int             DrawMultiLineString( const  SmtMultiLineString *pMultiLinestring);
		int             DrawLineSpline(const SmtSpline *pSpline);
		int             DrawMultiPoint( const  SmtStyle*pStyle, const  SmtMultiPoint *pMultiPoint);
		int             DrawMultiPolygon( const  SmtMultiPolygon *pMultiPolygon);

		int             DrawPoint( const  SmtStyle*pStyle, const  SmtPoint *pPoint);
		int             DrawAnno(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const SmtPoint *pPoint);
		int             DrawSymbol(HICON hIcon,long lHeight,long lWhidth,const SmtPoint *pPoint);

		int             DrawLineString( const  SmtLineString *pLinestring);
		int             DrawLinearRing( const  SmtLinearRing *pLinearRing);
		int             DrawPloygon( const  SmtPolygon *pPloygon);

		int             DrawTin(const SmtTin *pTin);
		int             DrawGrid( const  SmtGrid *pGrid);
		int             DrawArc( const  SmtArc *pArc);
		int             DrawFan( const  SmtFan *pFan);

	public:
		int             DrawEllipse(float left,float top,float right,float bottom,bool bDP = false);
		int             DrawRect(const fRect &rect,bool bDP = false);
		int             DrawLine(fPoint *pfPoints,int nCount,bool bDP = false);
		int				DrawLine(const fPoint &ptA,const fPoint &ptB,bool bDP = false);
		int             DrawText(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const fPoint &point,bool bDP = false);
		int             DrawImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType);
		int             StrethImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType);

	private:
		int             DrawTinLines(const SmtTin *pTin);
		int             DrawTinNodes(const SmtTin *pTin);

		int             DrawGridLines( const  SmtGrid *pGrid);
		int             DrawGridNodes( const  SmtGrid *pGrid);

	protected:
		string           m_strLogName;
		volatile bool	 m_bRendering;

		SmtRenderContex  m_smtRC;
		SmtRenderBuf	 m_smtRenderBuf;
		SmtRenderBuf	 m_smtSharedBuf;

		Viewport		 &m_virViewport1;			//虚拟视口，屏幕图像映射使用
		Viewport		 &m_virViewport2;			//虚拟视口，屏幕图像映射使用

		SmtBufPool		 m_bufPool;

	protected:
		HINSTANCE		 m_hInst;
		HWND			 m_hWnd;

		HFONT            m_hFont;
		HPEN             m_hPen;
		HBRUSH           m_hBrush;
		HICON            m_hIcon;

		HFONT            m_hOldFont;
		HPEN             m_hOldPen;
		HBRUSH           m_hOldBrush;
		HDC				 m_hCurDC;
		bool			 m_bCurUseStyle;
		bool			 m_bLockStyle;

		char             m_szAnno[2000];
		float            m_fAnnoAngle;
		int              m_nFeatureType;

		Smt2DRenderPra	 m_rdPra;
	};
}

#endif //_GDI_RENDERTHREAD_H