/*
File:    gdi_renderdevice.h

Desc:    SmtGdiRenderDevice,GDI ��ͼ��Ⱦ������
		 1.����Ⱦ�㣺Quick		2		Direct ֱ����DC�ϻ��ƣ�������פ��������ˢ��֮������ʧ
					 Dynamic	1
					 Map		0
		 2.���߳�

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GDI_RENDERDEVICE_H
#define _GDI_RENDERDEVICE_H

#include "base/core/core.h"
#include "legacy/render/bridge/renderdevice.h"
#include "sdb/feature/feature.h"
#include "sdb/map/map.h"
#include "algorithm/geo/geometry.h"
#include "legacy/render/gdi/gdi_renderbuf.h"
#include "legacy/render/gdi/gdi_bufpool.h"
#include <mutex>

using namespace base;
using namespace sdb;
using namespace geo;

namespace render
{
	class SmtGdiRenderThread;

	class SmtGdiRenderDevice:public SmtRenderDevice
	{
	public:
		SmtGdiRenderDevice(HINSTANCE hInst);
		virtual ~SmtGdiRenderDevice(void);

		int             Init(HWND hWnd,const char * logname);
		int             Destroy(void);
		int             Release(void);

		int             Resize(int orgx,int orgy,int cx,int cy);

	public:
		int				Lock();
		int				Unlock();

		int             Refresh(void);
		int             Refresh(const SmtMap *pMap,fRect rect);
		int             RefreshDirectly(const SmtMap *pMap,lRect rect,bool bRealTime = false);

		int             ZoomMove(const SmtMap *pMap,fPoint dbfPointOffset,bool bRealTime = false);
		int             ZoomScale(const SmtMap *pMap,lPoint orgPoint,float fscale,bool bRealTime = false);
		int             ZoomToRect(const SmtMap *pMap,fRect rect,bool bRealTime = false);

		int				Timer();

	public:
		int             LPToDP(float x,float y,long &X,long &Y) const ;
		int             DPToLP(LONG X,LONG Y,float &x,float &y) const ;
		int             LRectToDRect(const fRect &frect,lRect &lrect) const ;
		int             DRectToLRect(const lRect &lrect,fRect &frect) const ;

		int             ReRenderMapByProxy(const SmtMap *pMap,int x,int y,int w,int h,int op = R2_COPYPEN);
		int             ReRenderMapRealTime(const SmtMap *pMap,int x,int y,int w,int h,int op = R2_COPYPEN);

		int             RenderMap(void);

	public:
		int             BeginRender(eRDBufferLayer eMRDBufLyr,bool bClear = false,const SmtStyle*pStyle = NULL,int op = R2_COPYPEN);
		int             EndRender(eRDBufferLayer eMRDBufLyr);

		int             RenderMap(const SmtMap *pMap,int op = R2_COPYPEN);
		int             RenderLayer(const SmtLayer *pLayer,int op = R2_COPYPEN);				//1-16 R2_BLACK-R2_WHITE
		int             RenderLayer(OGRLayer *pLayer,int op = R2_COPYPEN);
		int             RenderLayer(const SmtRasterLayer *pLayer,int op = R2_COPYPEN);			//1-16 R2_BLACK-R2_WHITE
		int             RenderLayer(const SmtTileLayer *pLayer,int op = R2_COPYPEN);			//1-16 R2_BLACK-R2_WHITE
		int             RenderFeature(OGRFeature *pFeature,int op = R2_COPYPEN);
		int             RenderGeometry(const OGRGeometry *pGeom,const SmtStyle*pStyle,int op = R2_COPYPEN);

	public:
		int             DrawMultiLineString(const OGRMultiLineString *pMultiLinestring);
		int             DrawMultiPoint(const SmtStyle*pStyle,const OGRMultiPoint *pMultiPoint);
		int             DrawMultiPolygon(const OGRMultiPolygon *pMultiPolygon);

		int             DrawPoint(const SmtStyle*pStyle,const OGRPoint *pPoint);
		int             DrawAnno(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const OGRPoint *pPoint);
		int             DrawSymbol(HICON hIcon,long lHeight,long lWhidth,const OGRPoint *pPoint);

		int             DrawLineString(const OGRLineString *pLinestring);
		int             DrawLineSpline(const OGRLineString *pSpline);
		int             DrawLinearRing(const OGRLinearRing *pLinearRing);
		int             DrawPloygon(const OGRPolygon *pPloygon);

		int             DrawTin(const SmtTin *pTin);
		int             DrawGrid(const SmtGrid *pGrid);
		int             DrawArc(const OGRLineString *pArc);
		int             DrawFan(const OGRPolygon *pFan);

	public:
		int             DrawEllipse(float left,float top,float right,float bottom,bool bDP = false);
		int             DrawRect(const fRect &rect,bool bDP = false);
		int             DrawLine(fPoint *pfPoints,int nCount,bool bDP = false);
		int				DrawLine(const fPoint &ptA,const fPoint &ptB,bool bDP = false);
		int             DrawText(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const fPoint &point,bool bDP = false);
		int             DrawImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType,eRDBufferLayer eMRDBufLyr = MRD_BL_MAP);
		int             StrethImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType,eRDBufferLayer eMRDBufLyr = MRD_BL_MAP);

	public:
		int				SaveImage(const char * szFilePath,eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,bool bBgTransparent = false);
		int				Save2ImageBuf(char *&szImageBuf,long &lImageBufSize,long lCodeType,eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,bool bBgTransparent = false);
		int				FreeImageBuf(char *&szImageBuf);

	private:
		int             DrawTinLines(const SmtTin *pTin);
		int             DrawTinNodes(const SmtTin *pTin);

		int             DrawGridLines(const SmtGrid *pGrid);
		int             DrawGridNodes(const SmtGrid *pGrid);

	protected:
		int             PrepareForDrawing(const SmtStyle*pStyle,int nDrawMode = R2_COPYPEN);
		int             EndDrawing();

	protected:
		
		HFONT           m_hFont;
		HPEN            m_hPen;
		HBRUSH          m_hBrush;
		HICON           m_hIcon;

		HFONT           m_hOldFont;
		HPEN            m_hOldPen;
		HBRUSH          m_hOldBrush;

		HDC				m_hCurDC;
		bool			m_bCurUseStyle;
		bool			m_bLockStyle;
	
		char            m_szAnno[2000];
		float           m_fAnnoAngle;
		int             m_nFeatureType;
		
		Viewport		m_virViewport1;			//�����ӿڣ���Ļͼ��ӳ��ʹ��
		Viewport		m_virViewport2;			//�����ӿڣ���Ļͼ��ӳ��ʹ��

		LONGLONG		m_llLastRedrawCmdStamp;	//�ϴ��ػ������cpuʱ���
		LONGLONG		m_llLastRedrawStamp;	//�ϴ��ػ��cpuʱ���
		bool			m_bRedraw;				//��ʱ����
		 
		SmtRenderBuf	m_smtMapRenderBuf;
		SmtRenderBuf	m_smtDynamicRenderBuf;
		SmtRenderBuf	m_smtQuickRenderBuf;
		SmtRenderBuf	m_smtRenderBuf;

		SmtBufPool		m_bufPool;

	protected:
		SmtGdiRenderThread			*m_pRenderThread;

		std::mutex		m_cslock;
	};
}

#if !defined(RENDER_GDI_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif //_GDI_RENDERDEVICE_H