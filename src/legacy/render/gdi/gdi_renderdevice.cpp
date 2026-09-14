#include <math.h>

#include "legacy/render/gdi/gdi_renderdevice.h"
#include "base/core/log.h"
#include "sdb/carto/style_api.h"
#include "legacy/render/gdi/gdi_aux_api.h"
#include "legacy/render/gdi/resource.h"
#include "legacy/render/gdi/gdi_renderthread.h"
#include "base/core/api.h"
#include "ximage.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"
#include "ogrsf_frmts.h"
#include "legacy/render/bridge/leftover_record.h"

using namespace sdb;
using namespace base;
using namespace geo;

namespace render
{
	const float		C_fDELAY = 0.25;	

	int CreateRenderDevice(HINSTANCE hInst,LPRENDERDEVICE &pMrdDevice)
	{
		if(!pMrdDevice) 
		{
			pMrdDevice = new SmtGdiRenderDevice(hInst);

			return SMT_ERR_NONE;
		}
		return SMT_ERR_FAILURE;
	}

	int DestroyRenderDevice(LPRENDERDEVICE &pMrdDevice)
	{
		if(!pMrdDevice) 
			return SMT_ERR_FAILURE;

		SMT_SAFE_DELETE(pMrdDevice);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	SmtGdiRenderDevice::SmtGdiRenderDevice(HINSTANCE hInst):SmtRenderDevice(hInst)
		,m_hFont(NULL)
		,m_hPen(NULL)
		,m_hBrush(NULL)
		,m_hIcon(NULL)
		,m_hOldFont(NULL)
		,m_hOldPen(NULL)
		,m_hOldBrush(NULL)
		,m_hCurDC(NULL)
		,m_fAnnoAngle(0.)
		,m_nFeatureType(SmtFeatureType::SmtFtUnknown)
		,m_pRenderThread(NULL)
		,m_llLastRedrawCmdStamp(0)
		,m_llLastRedrawStamp(0)
		,m_bRedraw(false)
		,m_bCurUseStyle(false)
		,m_bLockStyle(false)
	{
		m_rBaseApi				 = RD_GDI;

		m_curDrawingOrg.x		 = 0;
		m_curDrawingOrg.y		 = 0;	
		sprintf(m_szAnno,"Smart Gis");

		m_pRenderThread= new SmtGdiRenderThread(hInst,m_virViewport1,m_virViewport2);
	}

	SmtGdiRenderDevice::~SmtGdiRenderDevice(void)
	{
       Release();
	}

	int SmtGdiRenderDevice::Init(HWND hWnd,const char * logname)
	{
	   if (hWnd == NULL || logname == NULL)
			return SMT_ERR_INVALID_PARAM;
		 
	   m_hWnd = hWnd;
	   bind_rhi_present(hWnd);


	   LOGGING(LOG_INFO, "Init Gdi SmtRenderDevice ok!");
	 
	   m_strLogName = logname;

	   m_smtRenderBuf.SetWnd(m_hWnd);
	   m_smtMapRenderBuf.SetWnd(m_hWnd);
	   m_smtDynamicRenderBuf.SetWnd(m_hWnd);
	   m_smtQuickRenderBuf.SetWnd(m_hWnd);
	   
	   m_pRenderThread->Init(m_hWnd,logname);
	   m_pRenderThread->start();
	   // Stay suspended until ReRenderMapByProxy/refresh. resume() here races
	   // CreateNewFrame's nested pump and hangs --self-test after OnCreate.
	 //  m_pRenderThread->resume();
	 //  m_pRenderThread->suspend();

	   return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::Destroy(void)
	{
		LOGGING(LOG_INFO, "Destroy Gdi SmtRenderDevice ok!");

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::Release(void)
	{
		// Stop the worker before touching GDI objects it may still reference.
		if (m_pRenderThread) {
			m_pRenderThread->stop();
		}

		LOGGING(LOG_INFO, "Release Gdi SmtRenderDevice ok!");

	   if (m_hFont)
	   {
		   DeleteObject(m_hFont);
		   m_hFont = NULL;
	   }

	   if (m_hPen)
	   {
		   DeleteObject(m_hPen);
		   m_hPen = NULL;
	   }

	   if (m_hBrush)
	   {
		   DeleteObject(m_hBrush);
		   m_hBrush = NULL;
	   }

	   if (m_hIcon)
	   {
		   DeleteObject(m_hIcon);
		   m_hIcon = NULL;
	   }

	   SMT_SAFE_DELETE(m_pRenderThread);
       return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::Resize(int orgx,int orgy,int cx,int cy)
	{
		if (cx < 0 || cy < 0)
			return SMT_ERR_FAILURE;
		 
		if (is_equal(m_Viewport.m_fVOX,orgx,dEPSILON) && 
			is_equal(m_Viewport.m_fVOY,orgy,dEPSILON) &&
			is_equal(m_Viewport.m_fVHeight,cy,dEPSILON) &&
			is_equal(m_Viewport.m_fVWidth,cx,dEPSILON) )
		{
			return SMT_ERR_FAILURE;
		}

		m_Viewport.m_fVOX = orgx;
		m_Viewport.m_fVOY = orgy;
		m_Viewport.m_fVHeight = cy;
		m_Viewport.m_fVWidth  = cx;

		m_virViewport1 = m_Viewport;
		m_virViewport2 = m_Viewport;

		float xblc,yblc;
		xblc = m_Viewport.m_fVWidth/m_Windowport.m_fWWidth;
		yblc = m_Viewport.m_fVHeight/m_Windowport.m_fWHeight;

		m_fblc = (xblc > yblc)?yblc:xblc;

		if (SMT_ERR_NONE == m_smtRenderBuf.SetBufSize(m_Viewport.m_fVWidth,m_Viewport.m_fVHeight) &&
			SMT_ERR_NONE == m_smtDynamicRenderBuf.SetBufSize(m_Viewport.m_fVWidth,m_Viewport.m_fVHeight) &&
			SMT_ERR_NONE == m_smtMapRenderBuf.SetBufSize(m_Viewport.m_fVWidth,m_Viewport.m_fVHeight) &&
			SMT_ERR_NONE == m_smtQuickRenderBuf.SetBufSize(m_Viewport.m_fVWidth,m_Viewport.m_fVHeight))
		{
			if (SMT_ERR_NONE == m_smtRenderBuf.SwapBuf(m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight,m_Viewport.m_fVOX,m_Viewport.m_fVOY) &&
				SMT_ERR_NONE == m_smtQuickRenderBuf.SwapBuf(m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight,m_Viewport.m_fVOX,m_Viewport.m_fVOY) &&
				SMT_ERR_NONE == m_smtDynamicRenderBuf.SwapBuf(m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight,m_Viewport.m_fVOX,m_Viewport.m_fVOY) &&
				SMT_ERR_NONE == m_smtMapRenderBuf.SwapBuf(m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight,m_Viewport.m_fVOX,m_Viewport.m_fVOY))
			{
				if (SMT_ERR_NONE == m_pRenderThread->Resize(orgx,orgy,cx,cy) &&
					SMT_ERR_NONE == m_pRenderThread->ShareBuf(m_smtMapRenderBuf))
				{
					return SMT_ERR_NONE;
				}
			}
		}

		return SMT_ERR_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::LPToDP(float x,float y,LONG &X,LONG &Y) const 
	{	
		if (is_equal(m_Windowport.m_fWWidth,0,dEPSILON) && 
			is_equal(m_Windowport.m_fWHeight,0,dEPSILON) &&
			is_equal(m_Viewport.m_fVWidth,0,dEPSILON) &&
			is_equal(m_Viewport.m_fVHeight,0,dEPSILON) )
		{
			X = x;
			Y = y;

			return SMT_ERR_FAILURE;
		}

		X=LONG(m_Viewport.m_fVOX+(x-m_Windowport.m_fWOX)*m_fblc + 0.5);
		Y=LONG(m_Viewport.m_fVOY+(y-m_Windowport.m_fWOY)*m_fblc + 0.5);

		Y = m_Viewport.m_fVHeight - Y;

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DPToLP(LONG X,LONG Y,float &x,float &y) const 
	{
		if (is_equal(m_Windowport.m_fWWidth,0,dEPSILON) && 
			is_equal(m_Windowport.m_fWHeight,0,dEPSILON) &&
			is_equal(m_Viewport.m_fVWidth,0,dEPSILON) &&
			is_equal(m_Viewport.m_fVHeight,0,dEPSILON) )
		{
			x = X;
			y = Y;

			return SMT_ERR_FAILURE;
		}

		Y = m_Viewport.m_fVHeight - Y;

		x = (X-m_Viewport.m_fVOX)/m_fblc + m_Windowport.m_fWOX;
		y = (Y-m_Viewport.m_fVOY)/m_fblc + m_Windowport.m_fWOY;

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::LRectToDRect(const fRect &frect,lRect &lrect) const 
	{
		LPToDP(frect.lb.x,frect.lb.y,lrect.lb.x,lrect.lb.y);
		LPToDP(frect.rt.x,frect.rt.y,lrect.rt.x,lrect.rt.y);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DRectToLRect(const lRect &lrect,fRect &frect) const 
	{
		DPToLP(lrect.lb.x,lrect.lb.y,frect.lb.x,frect.lb.y);
		DPToLP(lrect.rt.x,lrect.rt.y,frect.rt.x,frect.rt.y);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::Lock()
	{
		m_cslock.lock();
		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::Unlock()
	{
		m_cslock.unlock();
		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::Refresh()
	{
		//
		int invalidatex1, invalidatey1, invalidatew1, invalidateh1;
		int invalidatex2, invalidatey2, invalidatew2, invalidateh2;
		if (m_curDrawingOrg.x >= 0)
		{
			if (m_curDrawingOrg.y >= 0)
			{
				invalidatex1 = invalidatey1 = 0 ;
				invalidatew1 = m_Viewport.m_fVWidth, invalidateh1 = m_curDrawingOrg.y ;
				invalidatex2 = 0, invalidatey2 = m_curDrawingOrg.y ;
				invalidatew2 = m_curDrawingOrg.x, invalidateh2 = m_Viewport.m_fVHeight - m_curDrawingOrg.y ;
			}
			else
			{
				invalidatex1 = invalidatey1 = 0 ;
				invalidatew1 = m_curDrawingOrg.x, invalidateh1 = m_Viewport.m_fVHeight + m_curDrawingOrg.y ;
				invalidatex2 = 0, invalidatey2 = m_Viewport.m_fVHeight + m_curDrawingOrg.y ;
				invalidatew2 = m_Viewport.m_fVWidth, invalidateh2 = -m_curDrawingOrg.y ;
			}
		}
		else
		{
			if (m_curDrawingOrg.y >= 0)
			{
				invalidatex1 = invalidatey1 = 0 ;
				invalidatew1 = m_Viewport.m_fVWidth, invalidateh1 = m_curDrawingOrg.y ;
				invalidatex2 = m_Viewport.m_fVWidth + m_curDrawingOrg.x, invalidatey2 = m_curDrawingOrg.y ;
				invalidatew2 = -m_curDrawingOrg.x, invalidateh2 = m_Viewport.m_fVHeight - m_curDrawingOrg.y ;
			}
			else
			{
				invalidatex1 = m_Viewport.m_fVWidth + m_curDrawingOrg.x, invalidatey1 = 0 ;
				invalidatew1 = -m_curDrawingOrg.x, invalidateh1 = m_Viewport.m_fVHeight + m_curDrawingOrg.y ;
				invalidatex2 = 0, invalidatey2 = m_Viewport.m_fVHeight + m_curDrawingOrg.y ;
				invalidatew2 = m_Viewport.m_fVWidth, invalidateh2 = -m_curDrawingOrg.y ;
			}
		}

		HDC hDC = GetDC(m_hWnd);
		clear_rect(hDC,invalidatex1, invalidatey1, invalidatew1, invalidateh1/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);
		clear_rect(hDC,invalidatex2, invalidatey2, invalidatew2, invalidateh2/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		m_smtRenderBuf.ClearBuf(m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVWidth/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		m_smtMapRenderBuf.SwapBuf(m_smtRenderBuf,
			m_virViewport1.m_fVOX,m_virViewport1.m_fVOY,m_virViewport1.m_fVWidth,m_virViewport1.m_fVHeight,
			m_virViewport2.m_fVOX,m_virViewport2.m_fVOY,m_virViewport2.m_fVWidth,m_virViewport2.m_fVHeight,
			BLT_TRANSPARENT,SRCCOPY/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		m_smtDynamicRenderBuf.SwapBuf(m_smtRenderBuf,
			m_virViewport1.m_fVOX,m_virViewport1.m_fVOY,m_virViewport1.m_fVWidth,m_virViewport1.m_fVHeight,
			m_virViewport2.m_fVOX,m_virViewport2.m_fVOY,m_virViewport2.m_fVWidth,m_virViewport2.m_fVHeight,
			BLT_TRANSPARENT,SRCCOPY/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		m_smtQuickRenderBuf.SwapBuf(m_smtRenderBuf,
			m_virViewport1.m_fVOX,m_virViewport1.m_fVOY,m_virViewport1.m_fVWidth,m_virViewport1.m_fVHeight,
			m_virViewport2.m_fVOX,m_virViewport2.m_fVOY,m_virViewport2.m_fVWidth,m_virViewport2.m_fVHeight,
			BLT_TRANSPARENT,SRCCOPY/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);


		m_smtRenderBuf.SwapBuf(m_curDrawingOrg.x,m_curDrawingOrg.y,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight,m_Viewport.m_fVOX,m_Viewport.m_fVOY);

		::ReleaseDC(m_hWnd,hDC);

		RECT rt;
		GetClientRect(m_hWnd,&rt);
		InvalidateRect(m_hWnd,&rt,true);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::Refresh(const SmtMap *pMap,fRect frect)
	{
        lRect lrect;
		LRectToDRect(frect,lrect);
		RefreshDirectly(pMap,lrect);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::RefreshDirectly(const SmtMap *pSmtMap,lRect rect,bool bRealTime )
	{
		if (pSmtMap)
		{
			if(!bRealTime)
				return ReRenderMapByProxy(pSmtMap,m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight);
			else
				return ReRenderMapRealTime(pSmtMap,m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight);
		}

		return SMT_ERR_INVALID_PARAM;
	}

	int SmtGdiRenderDevice::ZoomMove(const SmtMap *pSmtMap,fPoint dbfPointOffset,bool bRealTime)
	{
		while(m_pRenderThread->IsRendering())Sleep(0);
		 
		m_virViewport1.m_fVOX += dbfPointOffset.x*m_fblc;
		m_virViewport1.m_fVOY -= dbfPointOffset.y*m_fblc;

		m_Windowport.m_fWOX -= dbfPointOffset.x;
		m_Windowport.m_fWOY -= dbfPointOffset.y;

		if (pSmtMap)
		{
			if(!bRealTime)
				return ReRenderMapByProxy(pSmtMap,m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight);
			else
				return ReRenderMapRealTime(pSmtMap,m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight);
		}

		return SMT_ERR_INVALID_PARAM;
	}

	int SmtGdiRenderDevice::ZoomScale(const SmtMap *pSmtMap,lPoint orgPoint,float fscale,bool bRealTime)
	{ 
		while(m_pRenderThread->IsRendering())Sleep(0);
	
		if(fscale > 1.)
		{//�ı�m_virViewport1
			m_virViewport1.m_fVHeight /= fscale;
			m_virViewport1.m_fVWidth  /= fscale;
			m_virViewport1.m_fVOX = orgPoint.x - (orgPoint.x-m_virViewport1.m_fVOX)/fscale;
			m_virViewport1.m_fVOY = orgPoint.y - (orgPoint.y-m_virViewport1.m_fVOY)/fscale;
		}
		else
		{//�ı�m_virViewport2
			m_virViewport2.m_fVHeight *= fscale;
			m_virViewport2.m_fVWidth  *= fscale;
			m_virViewport2.m_fVOX = orgPoint.x + (m_virViewport2.m_fVOX - orgPoint.x)*fscale;
			m_virViewport2.m_fVOY = orgPoint.y + (m_virViewport2.m_fVOY - orgPoint.y)*fscale;
		}
		
		float x1,y1,x2,y2;
		DPToLP(orgPoint.x,orgPoint.y,x1,y1);

		m_Windowport.m_fWHeight *= fscale;
		m_Windowport.m_fWWidth  *= fscale;

		m_fblc /= fscale;

		DPToLP(orgPoint.x,orgPoint.y,x2,y2);

		m_Windowport.m_fWOX -= x2-x1;
		m_Windowport.m_fWOY -= y2-y1;	

		if (pSmtMap)
		{
			if(!bRealTime)
				return ReRenderMapByProxy(pSmtMap,m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight);
			else
				return ReRenderMapRealTime(pSmtMap,m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight);
		}

		return SMT_ERR_INVALID_PARAM;
	}

	int SmtGdiRenderDevice::ZoomToRect(const SmtMap *pSmtMap,fRect rect,bool bRealTime)
	{
		while(m_pRenderThread->IsRendering())Sleep(0);

		lRect rt;
		LRectToDRect(rect,rt);
		m_virViewport2.m_fVOX = rt.lb.x;
		m_virViewport2.m_fVOY = rt.rt.y;
		m_virViewport2.m_fVHeight = rt.height();
		m_virViewport2.m_fVWidth = rt.width();
		
		m_Windowport.m_fWOX = rect.lb.x;
		m_Windowport.m_fWOY = rect.lb.y;
		m_Windowport.m_fWHeight = rect.height();
		m_Windowport.m_fWWidth  = rect.width(); 

		float xblc,yblc;
		xblc = m_Viewport.m_fVWidth/m_Windowport.m_fWWidth;
		yblc = m_Viewport.m_fVHeight/m_Windowport.m_fWHeight;

		m_fblc = (xblc > yblc)?yblc:xblc;

		if (xblc < yblc)
		{
			m_virViewport2.m_fVOY += rt.height()*(1-yblc/xblc);
			m_Windowport.m_fWHeight = rect.height()*yblc/xblc;
			m_virViewport2.m_fVHeight = rt.height()*yblc/xblc;
		}
		else
		{
			m_Windowport.m_fWWidth  = rect.width()*xblc/yblc; 
			m_virViewport2.m_fVWidth = rt.width()*xblc/yblc;
		}

		if (pSmtMap)
		{
			if(!bRealTime)
				return ReRenderMapByProxy(pSmtMap,m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight);
			else
				return ReRenderMapRealTime(pSmtMap,m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight);
		}

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderDevice::Timer()
	{
		if (m_bRedraw)
		{
			double	 dbfElapse = 0.;
			LONGLONG llStamp = 0,llPerCount = 0;

			QueryPerformanceFrequency((LARGE_INTEGER *) &llPerCount);
			QueryPerformanceCounter((LARGE_INTEGER *) &llStamp);

			dbfElapse = (llStamp - m_llLastRedrawCmdStamp)/(double)llPerCount;

			if (dbfElapse > C_fDELAY)
			{//�����ػ�
				m_bRedraw = false;
				m_pRenderThread->resume();							//������ͼ�߳�
			}
		}

		RECT rt;
		GetClientRect(m_hWnd,&rt);
		InvalidateRect(m_hWnd,&rt,true);

		return SMT_ERR_NONE;
	}
	
	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::BeginRender(eRDBufferLayer eMRDBufLyr,bool bClear,const SmtStyle*pStyle,int op)
	{
		m_bLockStyle = (NULL != pStyle);

		switch (eMRDBufLyr)
		{
		case MRD_BL_MAP:
			{
				if(bClear)
					m_smtMapRenderBuf.ClearBuf(m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

				m_hCurDC = m_smtMapRenderBuf.PrepareDC();
			}
			break;
		case MRD_BL_DYNAMIC:
			{
				if(bClear)
					m_smtDynamicRenderBuf.ClearBuf(m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

				m_hCurDC = m_smtDynamicRenderBuf.PrepareDC();
			}
			break;
		case MRD_BL_QUICK:
			{
				if(bClear)
					m_smtQuickRenderBuf.ClearBuf(m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

				m_hCurDC = m_smtQuickRenderBuf.PrepareDC();
			}
			break;
		case MRD_BL_DIRECT:
			{
				if(bClear)
				{
					RECT rt;
					GetClientRect(m_hWnd,&rt);
					InvalidateRect(m_hWnd,&rt,true);
				}

				m_hCurDC = GetDC(m_hWnd);
			}
			break;
		}

		if (m_bLockStyle)
			PrepareForDrawing(pStyle,op);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::EndRender(eRDBufferLayer eMRDBufLyr)
	{
		if(m_bLockStyle)
			EndDrawing();

		switch (eMRDBufLyr)
		{
		case MRD_BL_MAP:
			{
				m_smtMapRenderBuf.EndDC();
			}
			break;
		case MRD_BL_DYNAMIC:
			{
				m_smtDynamicRenderBuf.EndDC();
			}
			break;
		case MRD_BL_QUICK:
			{
				m_smtQuickRenderBuf.EndDC();
			}
			break;
		case MRD_BL_DIRECT:
			{
				ReleaseDC(m_hWnd,m_hCurDC);
			}
			break;
		}

		m_hCurDC = NULL;
		m_bLockStyle = false;

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::PrepareForDrawing(const SmtStyle*pStyle,int nDrawMode)
	{
		::SetROP2(m_hCurDC,nDrawMode);

		m_bCurUseStyle	= (NULL != pStyle);

		if (m_bCurUseStyle)
		{
			ulong format = pStyle->get_style_type();
			if( format & ST_PenDesc )
			{
				SmtPenDesc pen = pStyle->get_pen_desc();

				if (m_hPen)
				{
					DeleteObject(m_hPen);
					m_hPen = NULL;
				}

				m_hPen = CreatePen(pen.lPenStyle,pen.fPenWidth*m_fblc,pen.lPenColor);
				m_hOldPen = (HPEN)::SelectObject(m_hCurDC, m_hPen);
			}

			if( format & ST_BrushDesc )
			{
				SmtBrushDesc brush = pStyle->get_brush_desc();

				if (m_hBrush)
				{
					DeleteObject(m_hBrush);
					m_hBrush = NULL;
				}

				if(brush.brushTp == SmtBrushDesc::BT_Hatch)
				{
					m_hBrush = CreateHatchBrush(brush.lBrushStyle, brush.lBrushColor);
				}
				else
					m_hBrush = CreateSolidBrush(brush.lBrushColor);

				m_hOldBrush = (HBRUSH)::SelectObject(m_hCurDC, m_hBrush);
			}

			if (format & ST_SymbolDesc)
			{
				SmtSymbolDesc symbol = pStyle->get_symbol_desc();

				if (m_hIcon)
				{
					DeleteObject(m_hIcon);
					m_hIcon = NULL;
				}

				m_hIcon = LoadIcon(m_hInst,MAKEINTRESOURCE(symbol.lSymbolID+IDI_ICON_A));
			}

			if (format & ST_AnnoDesc)
			{
				SmtAnnotationDesc anno = pStyle->get_anno_desc();

				if (m_hFont)
				{
					DeleteObject(m_hFont);
					m_hFont = NULL;
				}

				m_hFont = CreateFont( anno.fHeight*m_fblc,anno.fWidth*m_fblc,anno.lEscapement,anno.lOrientation
					,anno.lWeight,anno.lItalic,anno.lUnderline,anno.lStrikeOut,anno.lCharSet,
					anno.lOutPrecision,anno.lClipPrecision,anno.lQuality,anno.lPitchAndFamily,anno.szFaceName);

				m_hOldFont  = (HFONT)::SelectObject(m_hCurDC,m_hFont);
				SetBkMode(m_hCurDC,TRANSPARENT);
				SetTextColor(m_hCurDC,anno.lAnnoClr);

				//m_fAnnoAngle = anno.fAngle;
			}
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::EndDrawing()
	{
		if (m_bCurUseStyle)
		{
			::SelectObject(m_hCurDC, m_hOldBrush);
			::SelectObject(m_hCurDC, m_hOldPen);
			::SelectObject(m_hCurDC, m_hOldFont);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::RenderMap(void)
	{
		m_smtRenderBuf.ClearBuf(m_Viewport.m_fVOX,m_Viewport.m_fVOY,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		m_smtMapRenderBuf.SwapBuf(m_smtRenderBuf,
			m_virViewport1.m_fVOX,m_virViewport1.m_fVOY,m_virViewport1.m_fVWidth,m_virViewport1.m_fVHeight,
			m_virViewport2.m_fVOX,m_virViewport2.m_fVOY,m_virViewport2.m_fVWidth,m_virViewport2.m_fVHeight,
			BLT_TRANSPARENT,SRCCOPY/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		m_smtDynamicRenderBuf.SwapBuf(m_smtRenderBuf,
			m_virViewport1.m_fVOX,m_virViewport1.m_fVOY,m_virViewport1.m_fVWidth,m_virViewport1.m_fVHeight,
			m_virViewport2.m_fVOX,m_virViewport2.m_fVOY,m_virViewport2.m_fVWidth,m_virViewport2.m_fVHeight,
			BLT_TRANSPARENT,SRCCOPY/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		m_smtQuickRenderBuf.SwapBuf(m_smtRenderBuf,
			m_virViewport1.m_fVOX,m_virViewport1.m_fVOY,m_virViewport1.m_fVWidth,m_virViewport1.m_fVHeight,
			m_virViewport2.m_fVOX,m_virViewport2.m_fVOY,m_virViewport2.m_fVWidth,m_virViewport2.m_fVHeight,
			BLT_TRANSPARENT,SRCCOPY/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		m_smtRenderBuf.SwapBuf(m_curDrawingOrg.x,m_curDrawingOrg.y,m_Viewport.m_fVWidth,m_Viewport.m_fVHeight,m_Viewport.m_fVOX,m_Viewport.m_fVOY);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::ReRenderMapByProxy( const SmtMap *pMap,int x,int y,int w,int h,int op)
	{
		if ( w == 0 || h == 0) 
			return SMT_ERR_INVALID_PARAM;
		
		SmtRenderContex smtRC(m_Viewport,m_Windowport,m_fblc,pMap,x,y,w,h,op);
		m_pRenderThread->SetRenderContex(smtRC);
		m_pRenderThread->SetRenderPra(m_rdPra);

		m_smtDynamicRenderBuf.ClearBuf(x,y,w,h/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);
		m_smtQuickRenderBuf.ClearBuf(x,y,w,h/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);
		
		if (!m_bRedraw)
		{
			m_bRedraw = true;
			QueryPerformanceCounter((LARGE_INTEGER *) &m_llLastRedrawStamp);
		}

		QueryPerformanceCounter((LARGE_INTEGER *) &m_llLastRedrawCmdStamp);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::ReRenderMapRealTime( const SmtMap *pMap,int x,int y,int w,int h,int op)
	{
		if ( w == 0 || h == 0) 
			return SMT_ERR_INVALID_PARAM;

		m_smtMapRenderBuf.ClearBuf(x,y,w,h/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);
		m_smtDynamicRenderBuf.ClearBuf(x,y,w,h/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);
		m_smtQuickRenderBuf.ClearBuf(x,y,w,h/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		m_hCurDC = m_smtMapRenderBuf.PrepareDC();
		RenderMap(pMap,op);
		m_smtMapRenderBuf.EndDC();
		m_hCurDC = NULL;

		Refresh();

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::RenderMap(const SmtMap *pMap,int op)
	{
		if (NULL == pMap)
			return SMT_ERR_INVALID_PARAM;

		// Skip leftover RHI on the GDI HWND (STATUS_FATAL_APP_EXIT when D3D
		// shares the MDI child). OGR/leftover vectors paint via RenderLayer.
		for (int i = 0; i < pMap->GetLayerCount(); ++i) {
			if (!pMap->IsLayerVisible(i)) {
				continue;
			}
			if (OGRLayer* ogr = const_cast<OGRLayer*>(pMap->GetOgrLayer(i))) {
				RenderLayer(ogr, op);
			} else {
				RenderLayer(pMap->GetLeftoverLayer(i), op);
			}
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::RenderLayer(const SmtLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		if(pLayer->GetLayerType() == LYR_RASTER)
			return RenderLayer((SmtRasterLayer*)pLayer,op);
		else if(pLayer->GetLayerType() == LYR_TITLE)
			return RenderLayer((SmtTileLayer*)pLayer,op);

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderDevice::RenderLayer(OGRLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		Envelope envLayer;
		OGREnvelope ogr_env;
		if (pLayer->GetExtent(&ogr_env, TRUE) == OGRERR_NONE) {
			envLayer.MinX = ogr_env.MinX;
			envLayer.MinY = ogr_env.MinY;
			envLayer.MaxX = ogr_env.MaxX;
			envLayer.MaxY = ogr_env.MaxY;
		}
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;

		viewport_to_rect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);

		if (!envLayer.intersects(envViewp))
			return SMT_ERR_NONE;

		pLayer->ResetReading();
		while (OGRFeature* feat = pLayer->GetNextFeature())
		{
			RenderFeature(feat,op);
			OGRFeature::DestroyFeature(feat);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::RenderLayer(const SmtRasterLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		if (!pLayer->IsVisible())
			return SMT_ERR_NONE; 

		Envelope envLayer ;
		pLayer->get_envelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;

		viewport_to_rect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);

		if (!envLayer.intersects(envViewp))
			return SMT_ERR_NONE;

		char		*pRasterBuf = NULL;
		long		lRasterBufSize = 0;
		long		lCodeType = -1;
		fRect		locRect;

		if (SMT_ERR_NONE == pLayer->GetRasterNoClone(pRasterBuf,lRasterBufSize,locRect,lCodeType))
		{
			lRect lrt;
			LRectToDRect(locRect,lrt);

			CxImage tmpImage;
			tmpImage.Decode((BYTE*)pRasterBuf,lRasterBufSize,lCodeType);
			tmpImage.Stretch(m_hCurDC,lrt.lb.x,lrt.rt.y,lrt.width(),lrt.height());
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::RenderLayer(const SmtTileLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		if (!pLayer->IsVisible())
			return SMT_ERR_NONE; 

		Envelope envLayer ;
		pLayer->get_envelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;
		lRect titleDPRect;

		viewport_to_rect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);

		if (!envLayer.intersects(envViewp))
			return SMT_ERR_NONE;

		pLayer->MoveFirst();
		while (!pLayer->IsEnd())
		{
			SmtTile *pTile = pLayer->GetTile();
			if (NULL != pTile && pTile->bVisible)
			{
			/*	Envelope envTile,envViewp;
				rect_to_envelope(envTile,pTile->rtTileRect);
				LRectToDRect(pTile->rtTileRect,titleDPRect);

				if (!envTile.intersects(envViewp) ||
					(titleDPRect.height() < 2 && titleDPRect.width() < 2))
					return SMT_ERR_NONE;*/

				CxImage tmpImage;
				tmpImage.Decode((BYTE*)pTile->pTileBuf,pTile->lTileBufSize,pTile->lImageCode);
				tmpImage.Stretch(m_hCurDC,titleDPRect.lb.x,titleDPRect.rt.y,titleDPRect.width(),titleDPRect.height());
			}			

			pLayer->MoveNext();
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::RenderFeature(OGRFeature *pFeature,int op)
	{
		if (NULL == pFeature)
			return SMT_ERR_INVALID_PARAM;

		m_nFeatureType = sdb::datasource::infer_feature_type(
			pFeature, SmtFeatureType::SmtFtUnknown);
		SmtStyle* pStyle = sdb::datasource::copy_ogr_style_from_ogr(pFeature);
		OGRGeometry* pGeom =
			sdb::datasource::decode_ogr_geometry(pFeature, static_cast<SmtFeatureType>(m_nFeatureType));

		if (m_nFeatureType == SmtFeatureType::SmtFtAnno) {
			const int ai = pFeature->GetFieldIndex("anno");
			const int gi = pFeature->GetFieldIndex("angle");
			if (ai >= 0) {
				sprintf(m_szAnno, "%s", pFeature->GetFieldAsString(ai));
			}
			if (gi >= 0) {
				m_fAnnoAngle = static_cast<float>(pFeature->GetFieldAsDouble(gi));
			}
		}

		const int rc = RenderGeometry(pGeom, pStyle, op);
		delete pGeom;
		delete pStyle;
		return rc;
	}

	int SmtGdiRenderDevice::RenderGeometry(const OGRGeometry *pGeom,const SmtStyle*pStyle,int op)
	{
		if (!pGeom)
			return SMT_ERR_INVALID_PARAM;

		const OGRwkbGeometryType type = wkbFlatten(pGeom->getGeometryType());

		Envelope envFeature,envViewp;
		geo::copy_envelope(*pGeom, &envFeature);

		lRect lViewp;
		fRect fViewp;
		fRect fenv;
		lRect lenv;

		viewport_to_rect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);

		envelope_to_rect(fenv,envFeature);
		LRectToDRect(fenv,lenv);

		if (!envFeature.intersects(envViewp) ||
			(type != wkbPoint && lenv.height() < 2 && lenv.width() < 2))
			return SMT_ERR_NONE;

		::SaveDC(m_hCurDC);

		if (!m_bLockStyle)
			PrepareForDrawing(pStyle);
		
		if (m_rdPra.bShowMBR)
		{
			long lX = 0,lY = 0; 

			LPToDP(envFeature.MinX, envFeature.MinY,lX,lY);
			MoveToEx(m_hCurDC,lX,lY,NULL);

			LPToDP(envFeature.MaxX, envFeature.MinY,lX,lY);
			LineTo(m_hCurDC,lX,lY);

			LPToDP(envFeature.MaxX, envFeature.MaxY,lX,lY);
			LineTo(m_hCurDC,lX,lY);

			LPToDP(envFeature.MinX, envFeature.MaxY,lX,lY);
			LineTo(m_hCurDC,lX,lY) ;

			LPToDP(envFeature.MinX, envFeature.MinY,lX,lY);
			LineTo(m_hCurDC,lX,lY) ;        
		}

		switch (type) {
		case wkbPoint:
			DrawPoint(pStyle, (OGRPoint*)pGeom);
			break;
		case wkbLineString:
			DrawLineString((OGRLineString*)pGeom);
			break;
		case wkbPolygon:
		case wkbTriangle:
			DrawPloygon((OGRPolygon*)pGeom);
			break;
		case wkbMultiPoint:
			DrawMultiPoint(pStyle, (OGRMultiPoint*)pGeom);
			break;
		case wkbMultiLineString:
			DrawMultiLineString((OGRMultiLineString*)pGeom);
			break;
		case wkbMultiPolygon:
		case wkbTIN:
			DrawMultiPolygon((OGRMultiPolygon*)pGeom);
			break;
		case wkbLinearRing:
			DrawLinearRing((OGRLinearRing*)pGeom);
			break;
		default:
			break;
		}

		if(!m_bLockStyle)
			EndDrawing();

		::RestoreDC(m_hCurDC,-1);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::DrawMultiLineString(const OGRMultiLineString *pMultiLinestring)
	{
		int nLines = pMultiLinestring->getNumGeometries();

		int i = 0;
		while (i < nLines)
		{
			DrawLineString((OGRLineString*)pMultiLinestring->getGeometryRef(i));
			i++;
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawMultiPoint(const SmtStyle*pStyle,const OGRMultiPoint *pMultiPoint)
	{
		int nPoints = pMultiPoint->getNumGeometries();

		int i = 0;
		while (i < nPoints)
		{
			DrawPoint(pStyle,(OGRPoint*)pMultiPoint->getGeometryRef(i));
			i++;
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawMultiPolygon(const OGRMultiPolygon *pMultiPolygon)
	{
		int nPolygons = pMultiPolygon->getNumGeometries();

		int i = 0;
		while (i < nPolygons)
		{
			DrawPloygon((OGRPolygon*)pMultiPolygon->getGeometryRef(i));
			i++;
		}

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::DrawPoint(const SmtStyle*pStyle,const OGRPoint *pPoint)
	{
		ulong format = pStyle->get_style_type();
		if (m_nFeatureType == SmtFeatureType::SmtFtAnno)
		{
			SmtAnnotationDesc anno = pStyle->get_anno_desc();
			return DrawAnno(m_szAnno,m_fAnnoAngle,abs(anno.fHeight),abs(anno.fWidth),abs(anno.fSpace),pPoint);
		}
		else if(m_nFeatureType == SmtFeatureType::SmtFtChildImage)
		{
			SmtSymbolDesc symbol = pStyle->get_symbol_desc();
			return DrawSymbol(m_hIcon,symbol.fSymbolHeight,symbol.fSymbolWidth,pPoint);
		}
		else if (m_nFeatureType == SmtFeatureType::SmtFtDot)
		{
			int r = m_rdPra.lPointRaduis/**m_fblc*/;
			long lX,lY;
			LPToDP(pPoint->getX(),pPoint->getY(),lX,lY);
			Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);

			if (m_rdPra.bShowPoint)
			{
				//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
				draw_cross(m_hCurDC,lX,lY,r);
			}

			return SMT_ERR_NONE;
		}	

		return SMT_ERR_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::DrawAnno(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const OGRPoint *pPoint)
	{
		if (szAnno == NULL)
			return SMT_ERR_INVALID_PARAM;

		fCHeight *= m_fblc;
		fCWidth *= m_fblc;
		fCSpace *= m_fblc;

		unsigned char c1,c2;
		fPoint pt;
		long x,y;
		char bz[4];
		const char *ls1;
		ls1 = szAnno;

		LPToDP(pPoint->getX(),pPoint->getY(),x,y);
		pt.x = x;
		pt.y = y;

		pt.x -= 2*fCHeight*sin(fangel);
		pt.y -= 2*fCHeight*cos(fangel);

		int nStrLength  = (int)strlen(ls1);
		while(nStrLength > 0)
		{
			c1 = *ls1;
			c2 = *(ls1 + 1);
			if(c1 >127 && c2 > 127) { // �����һ���ַ��Ǻ���?
				strncpy(bz,ls1,2);
				bz[2] = 0;
				ls1 = ls1 + 2;
				TextOut(m_hCurDC,pt.x,pt.y,(LPCSTR)bz,2);
				nStrLength -= 2;
				pt.x += (fCWidth*2 + fCSpace) * cos(fangel);
				pt.y += (fCWidth*2 + fCSpace) * sin(fangel);
			}
			else
			{
				strncpy(bz,ls1,1);
				bz[1] = 0;
				ls1++;
				TextOut(m_hCurDC,pt.x,pt.y,(LPCSTR)bz,1);
				nStrLength -= 1;

				pt.x += (fCWidth + fCSpace/2.) * cos(fangel);
				pt.y += (fCWidth + fCSpace/2.) * sin(fangel);
			}
		}

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			long lX,lY;
			LPToDP(pPoint->getX(),pPoint->getY(),lX,lY);
			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			draw_cross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawSymbol(HICON hIcon,long lHeight,long lWidth,const OGRPoint *pPoint)
	{
		lHeight *= m_fblc;
		lWidth *= m_fblc;

		long lX,lY;
		LPToDP(pPoint->getX(),pPoint->getY(),lX,lY);
		//::DrawIcon(m_hCurDC,pt.x-lWidth,pt.y-lHeight,hIcon);
		::DrawIconEx(m_hCurDC,lX-lWidth/2,lY+lHeight/2,   hIcon, lWidth, lHeight, 0, NULL, DI_NORMAL);
		//::DrawState(m_hCurDC,NULL,NULL,(LPARAM)hIcon,0,pt.x-lWidth/2,pt.y+lHeight/2,lWidth,lHeight, DSS_NORMAL | DST_ICON);

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			long lX,lY;
			LPToDP(pPoint->getX(),pPoint->getY(),lX,lY);
			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			draw_cross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawLineSpline(const OGRLineString *pSpline)
	{
		int    nPoints = pSpline->getNumPoints();
		if (nPoints < 2)
			return SMT_ERR_INVALID_PARAM;

		int i = 0;
		POINT *lpPoint = NULL;

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nPoints];
#else
		lpPoint = new POINT[nPoints];
#endif

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pSpline->getX(i),pSpline->getY(i),lpPoint[i].x,lpPoint[i].y);
				draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}

			MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
			PolylineTo(m_hCurDC, lpPoint, nPoints);
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pSpline->getX(i),pSpline->getY(i),lpPoint[i].x,lpPoint[i].y);
			}

			MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
			PolylineTo(m_hCurDC, lpPoint, nPoints);
		}

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif
		
		int r = m_rdPra.lPointRaduis;
		long lX,lY;
		for (int i = 0; i < pSpline->getNumPoints();i++)
		{
			LPToDP(pSpline->getX(i),pSpline->getY(i),lX,lY);
			Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawLineString(const OGRLineString *pLinestring)
	{
		int    nPoints = pLinestring->getNumPoints();
		if (nPoints < 2)
			return SMT_ERR_INVALID_PARAM;

		int i = 0;
		POINT *lpPoint = NULL;

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nPoints];
#else
		lpPoint = new POINT[nPoints];
#endif

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinestring->getX(i),pLinestring->getY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinestring->getX(i),pLinestring->getY(i),lpPoint[i].x,lpPoint[i].y);
			}
		}
		
		MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
		PolylineTo(m_hCurDC, lpPoint, nPoints);

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawLinearRing(const OGRLinearRing *pLinearRing)
	{
		int    nPoints = pLinearRing->getNumPoints();
		if (nPoints < 2)
			return SMT_ERR_INVALID_PARAM;

		int i = 0;
		POINT * lpPoint = NULL;
#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < m_bufPool.GetPoolSize())
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nPoints];
#else
		lpPoint = new POINT[nPoints];
#endif
		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			for (int i = 0; i < pLinearRing->getNumPoints();i++)
			{
				LPToDP(pLinearRing->getX(i),pLinearRing->getY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinearRing->getX(i),pLinearRing->getY(i),lpPoint[i].x,lpPoint[i].y);
			}
		}
		
		MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
		PolylineTo(m_hCurDC, lpPoint, nPoints);

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < m_bufPool.GetPoolSize())
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawPloygon(const OGRPolygon *pPloygon)
	{
		int nAllPts = 0;
		const OGRLinearRing *pLinerring = pPloygon->getExteriorRing();

		int    nExteriorPts = pLinerring->getNumPoints();
		if (nExteriorPts < 2)
			return SMT_ERR_INVALID_PARAM;

		BOOL bRet = FALSE;

		nAllPts += nExteriorPts;

		int nInteriorRings = pPloygon->getNumInteriorRings();
		int *nRings = new int[nInteriorRings + 1];
		nRings[0] = nExteriorPts;

		for (int i = 0; i < nInteriorRings ; i++)
		{
			const OGRLinearRing *pInteriorRing = pPloygon->getInteriorRing(i);
			nRings[i+1] = pInteriorRing->getNumPoints();
			nAllPts += nRings[i+1];
		}

		int i = 0;
		POINT * lpPoint = NULL;

#ifdef GDI_USE_BUFPOOL
		if (nAllPts*(sizeof(POINT)) < m_bufPool.GetPoolSize())
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nAllPts];
#else
		lpPoint = new POINT[nAllPts];
#endif
		
		int nCount = 0;
		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			for (int i = 0;i < nExteriorPts ; i++,nCount++)
			{
				LPToDP(pLinerring->getX(i),pLinerring->getY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}

			for (int i = 0; i < nInteriorRings ;i++)
			{
				const OGRLinearRing *pInteriorRing = pPloygon->getInteriorRing(i);
				int nInteriorPts= pInteriorRing->getNumPoints();
				for ( int j=0; j<nInteriorPts; ++j,nCount++)
				{
					LPToDP(pInteriorRing->getX(i),pInteriorRing->getY(i),lpPoint[i].x,lpPoint[i].y);
					//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
				}
			}

			bRet = ::PolyPolygon(m_hCurDC,lpPoint,nRings,nInteriorRings+1);
		}
		else
		{
			for (int i = 0;i < nExteriorPts ; i++,nCount++)
			{
				LPToDP(pLinerring->getX(i),pLinerring->getY(i),lpPoint[i].x,lpPoint[i].y);
			}

			for (int i = 0; i < nInteriorRings ;i++)
			{
				const OGRLinearRing *pInteriorRing = pPloygon->getInteriorRing(i);
				int nInteriorPts= pInteriorRing->getNumPoints();
				for ( int j=0; j<nInteriorPts; ++j,nCount++)
				{
					LPToDP(pInteriorRing->getX(i),pInteriorRing->getY(i),lpPoint[i].x,lpPoint[i].y);
				}
			}

			bRet = ::PolyPolygon(m_hCurDC,lpPoint,nRings,nInteriorRings+1);
		}
		
#ifdef GDI_USE_BUFPOOL
		if (nAllPts*(sizeof(POINT)) < m_bufPool.GetPoolSize())
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif

		SMT_SAFE_DELETE_A(nRings);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::DrawTin(const SmtTin *pTin)
	{
		DrawTinLines(pTin);

		//if (m_rdPra.bShowPoint)
		{
			DrawTinNodes(pTin);
		}	

		return SMT_ERR_NONE;
	}

	//����Tin��
	int SmtGdiRenderDevice::DrawTinLines(const SmtTin *pTin)
	{
		POINT	 lPt1,lPt2,lPt3;
		OGRPoint oPt1,oPt2,oPt3;

		Envelope envTri,envViewp;
		lRect lViewp;
		fRect fViewp;

		viewport_to_rect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);
	
		for (int i = 0; i < pTin->get_triangle_count();i++)
		{
			SmtTriangle tri = pTin->get_triangle(i);

			if (!tri.bDelete)
			{
				oPt1 = pTin->get_point(tri.a);
				oPt2 = pTin->get_point(tri.b);
				oPt3 = pTin->get_point(tri.c);

				envTri.merge(oPt1.getX(),oPt1.getY());
				envTri.merge(oPt2.getX(),oPt2.getY());
				envTri.merge(oPt3.getX(),oPt3.getY());

				if (envTri.intersects(envViewp))
				{
					LPToDP(oPt1.getX(),oPt1.getY(),lPt1.x,lPt1.y);
					LPToDP(oPt2.getX(),oPt2.getY(),lPt2.x,lPt2.y);
					LPToDP(oPt3.getX(),oPt3.getY(),lPt3.x,lPt3.y);

					MoveToEx(m_hCurDC,lPt1.x, lPt1.y, NULL);
					LineTo(m_hCurDC,lPt2.x,lPt2.y);
					LineTo(m_hCurDC,lPt3.x,lPt3.y);
					LineTo(m_hCurDC,lPt1.x,lPt1.y);
				}
			}
		}

		return SMT_ERR_NONE;
	}

	//����Tin�ڵ�
	int SmtGdiRenderDevice::DrawTinNodes(const SmtTin *pTin)
	{
		POINT		lPt;
		OGRPoint	oPt;
		int			r = m_rdPra.lPointRaduis;

		lRect lViewp;
		fRect fViewp;

		viewport_to_rect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		adjust_f_rect(fViewp);

		for (int i = 0; i < pTin->get_point_count(); i++)
		{
			oPt = pTin->get_point(i);
			if (is_in_f_rect(oPt.getX(),oPt.getY(),fViewp) )
			{
				LPToDP(oPt.getX(),oPt.getY(),lPt.x,lPt.y);
				Ellipse(m_hCurDC,lPt.x - r ,lPt.y - r,lPt.x + r ,lPt.y + r);
			}
		}

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int  SmtGdiRenderDevice::DrawGrid(const SmtGrid *pGrid)
	{
		DrawGridLines(pGrid);

		//if (m_rdPra.bShowPoint)
		{
			DrawGridNodes(pGrid);
		}	

		return SMT_ERR_NONE;
	}

	//����������
	int SmtGdiRenderDevice::DrawGridLines(const SmtGrid *pGrid)
	{
		int nM,nN;
		pGrid->get_size(nM,nN);

		POINT lPt;

		for (int j = 0; j < nN; j ++)
		{//������
			RawPoint rawPt = pGrid->node(0,j);
			LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
			MoveToEx(m_hCurDC,lPt.x, lPt.y, NULL);
			for (int i = 0; i < nM; i++)
			{//������
				RawPoint rawPt1 = pGrid->node(i,j);
				LPToDP(rawPt1.x,rawPt1.y,lPt.x,lPt.y);
				LineTo(m_hCurDC,lPt.x,lPt.y);
			}	 
		}

		for (int i = 0; i < nM ; i ++)
		{//������
			RawPoint rawPt = pGrid->node(i,0);
			LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
			MoveToEx(m_hCurDC,lPt.x, lPt.y, NULL);

			for (int j = 0; j < nN ;j ++)
			{//������
				RawPoint rawPt1 = pGrid->node(i,j);
				LPToDP(rawPt1.x,rawPt1.y,lPt.x,lPt.y);
				LineTo(m_hCurDC,lPt.x,lPt.y);
			}
		}

		return SMT_ERR_NONE;
	}

	// ��������ڵ�?
	int SmtGdiRenderDevice::DrawGridNodes(const SmtGrid *pGrid)
	{
		int nM,nN;
		pGrid->get_size(nM,nN);

		int r = m_rdPra.lPointRaduis;
		POINT lPt;

		for (int j = 0; j < nN; j ++)
		{
			for (int i = 0; i < nM; i++)
			{
				RawPoint rawPt = pGrid->node(i,j);
				LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
				Ellipse(m_hCurDC,lPt.x - r ,lPt.y - r,lPt.x + r ,lPt.y + r);
			}
		}

		return SMT_ERR_NONE;
	}

    int  SmtGdiRenderDevice::DrawFan(const OGRPolygon *pFan)
	{
		return DrawPloygon(pFan);
	}

	int  SmtGdiRenderDevice::DrawArc(const OGRLineString *pArc)
	{
		return DrawLineString(pArc);
#if 0
		long x1,y1,x2,y2,x3,y3,x4,y4,x5,y5;
        OGRPoint oStPoint,oEdbfPoint,oCtPoint;

		pArc->StartPoint(&oStPoint);
		pArc->EndPoint(&oEdbfPoint);
		pArc->GetCenterPoint(&oCtPoint);

		LPToDP(oStPoint.getX(),oStPoint.getY(),x3,y3);
		LPToDP(oEdbfPoint.getX(),oEdbfPoint.getY(),x4,y4);
		LPToDP(oCtPoint.getX(),oCtPoint.getY(),x5,y5);

		int dr = m_rdPra.lPointRaduis;
		float r = static_cast<float>(hypot(x5 - x4, y5 - y4));

		x1 = x5 - r;
		y1 = y5 - r;
		x2 = x5 + r;
		y2 = y5 + r;

		//::MoveToEx(m_hCurDC,x4,y4,NULL);
		//LineTo(m_hCurDC,x5,y5);
		//LineTo(m_hCurDC,x3,y3);
		::Arc(m_hCurDC,x1,y1,x2,y2,x3,y3,x4,y4);

		Ellipse(m_hCurDC,x3 - dr ,y3 - dr,x3 + dr ,y3 + dr);
		Ellipse(m_hCurDC,x4 - dr ,y4 - dr,x4 + dr ,y4 + dr);
		Ellipse(m_hCurDC,x5 - dr ,y5 - dr,x5 + dr ,y5 + dr);

		if (m_rdPra.bShowPoint)
		{
			//Rectangle(m_hCurDC,x3 - dr ,y3 - dr,x3 + dr ,y3 + dr);
			//Rectangle(m_hCurDC,x4 - dr ,y4 - dr,x4 + dr ,y4 + dr);
			//Rectangle(m_hCurDC,x5 - dr ,y5 - dr,x5 + dr ,y5 + dr);

			draw_cross(m_hCurDC,x3,y3,dr);
			draw_cross(m_hCurDC,x4,y4,dr);
			draw_cross(m_hCurDC,x5,y5,dr);
		}

		return SMT_ERR_NONE;
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::DrawEllipse(float left,float top,float right,float bottom,bool bDP )
	{
		lRect lrect;

		fRect frect;
		frect.lb.x = left;
		frect.lb.y = bottom;
		frect.rt.x = right;
		frect.rt.y = top;

		if (!bDP)
			LRectToDRect(frect,lrect);
		else
			f_rect_to_l_rect(lrect,frect);

		Ellipse(m_hCurDC,lrect.lb.x ,lrect.lb.y,lrect.rt.x ,lrect.rt.y);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawRect(const fRect &rect,bool bDP )
	{
		lRect tmpRectDP;

		f_rect_to_l_rect(tmpRectDP,rect);

		if (!bDP)
		{
			fRect tmpRectLP;

			l_rect_to_f_rect(tmpRectLP,tmpRectDP);
			LRectToDRect(tmpRectLP,tmpRectDP);
		}

		MoveToEx(m_hCurDC,tmpRectDP.lb.x,tmpRectDP.lb.y, NULL) ;
		LineTo(m_hCurDC,tmpRectDP.rt.x,tmpRectDP.lb.y);
		LineTo(m_hCurDC,tmpRectDP.rt.x,tmpRectDP.rt.y);
		LineTo(m_hCurDC,tmpRectDP.lb.x,tmpRectDP.rt.y);
		LineTo(m_hCurDC,tmpRectDP.lb.x,tmpRectDP.lb.y);
		
		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawLine(fPoint *pfPoints,int nCount,bool bDP)
	{
		int    nPoints = nCount;
		if (nPoints < 2)
			return SMT_ERR_INVALID_PARAM;

		int i = 0;
		POINT *lpPoint = NULL;

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nPoints];
#else
		lpPoint = new POINT[nPoints];
#endif

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			if (!bDP)
			{
				for (int i = 0;i < nPoints ; i++)
				{
					LPToDP(pfPoints[i].x,pfPoints[i].y,lpPoint[i].x,lpPoint[i].y);
					//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
				}
			}
			else
			{
				for (int i = 0;i < nPoints ; i++)
				{
					lpPoint[i].x = pfPoints[i].x;
					lpPoint[i].y = pfPoints[i].y;
					//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
				}
			}
		}
		else
		{
			if (!bDP)
			{
				for (int i = 0;i < nPoints ; i++)
				{
					LPToDP(pfPoints[i].x,pfPoints[i].y,lpPoint[i].x,lpPoint[i].y);
				}
			}
			else
			{
				for (int i = 0;i < nPoints ; i++)
				{
					lpPoint[i].x = pfPoints[i].x;
					lpPoint[i].y = pfPoints[i].y;
				}
			}	
		}

		MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
		PolylineTo(m_hCurDC, lpPoint, nPoints);

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawLine(const fPoint &ptA,const fPoint &ptB,bool bDP)
	{
		lPoint pt1(ptA.x,ptA.y),pt2(ptB.x,ptB.y);

		if (!bDP)
		{
			LPToDP(ptA.x,ptA.y,pt1.x,pt1.y);
			LPToDP(ptB.x,ptB.y,pt2.x,pt2.y);
		}

		MoveToEx(m_hCurDC,pt1.x,pt1.y, NULL) ;
		LineTo(m_hCurDC,pt2.x,pt2.y);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawText(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const fPoint &point,bool bDP)
	{
		if (szAnno == NULL)
			return SMT_ERR_INVALID_PARAM;

		fCHeight *= m_fblc;
		fCWidth *= m_fblc;
		fCSpace *= m_fblc;

		unsigned char c1,c2;
		fPoint pt;
		long x,y;
		char bz[4];
		const char *ls1;
		ls1 = szAnno;

		lRect tmpRectDP;

		if (!bDP)
		{
			LPToDP(point.x,point.y,x,y);
			pt.x = x;
			pt.y = y;
		}
		else
		{
			pt.x = point.x;
			pt.y = point.y;
		}

		pt.x -= 2*fCHeight*sin(fangel);
		pt.y -= 2*fCHeight*cos(fangel);

		int nStrLength  = (int)strlen(ls1);
		while(nStrLength > 0)
		{
			c1 = *ls1;
			c2 = *(ls1 + 1);
			if(c1 >127 && c2 > 127) { // �����һ���ַ��Ǻ���?
				strncpy(bz,ls1,2);
				bz[2] = 0;
				ls1 = ls1 + 2;
				TextOut(m_hCurDC,pt.x,pt.y,(LPCSTR)bz,2);
				nStrLength -= 2;
				pt.x += (fCWidth*2 + fCSpace) * cos(fangel);
				pt.y += (fCWidth*2 + fCSpace) * sin(fangel);
			}
			else
			{
				strncpy(bz,ls1,1);
				bz[1] = 0;
				ls1++;
				TextOut(m_hCurDC,pt.x,pt.y,(LPCSTR)bz,1);
				nStrLength -= 1;

				pt.x += (fCWidth + fCSpace/2.) * cos(fangel);
				pt.y += (fCWidth + fCSpace/2.) * sin(fangel);
			}
		}

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			long lX,lY;
			if (!bDP)
			{
				LPToDP(point.x,point.y,lX,lY);
			}
			else
			{
				lX = point.x;
				lY = point.y;
			}

			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			draw_cross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType,eRDBufferLayer eMRDBufLyr)
	{
		long lRtn = SMT_ERR_FAILURE;

		lRect lrt;
		LRectToDRect(frect,lrt);

		switch (eMRDBufLyr)
		{
		case MRD_BL_MAP:
			{
				lRtn = m_smtMapRenderBuf.DrawImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.width(),lrt.height());
			}
			break;

		case MRD_BL_DYNAMIC:
			{
				lRtn = m_smtDynamicRenderBuf.DrawImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.width(),lrt.height());
			}
			break;

		case MRD_BL_QUICK:
			{
				lRtn = m_smtQuickRenderBuf.DrawImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.width(),lrt.height());
			}
			break;
		}

		return lRtn;
	}

	int SmtGdiRenderDevice::StrethImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType,eRDBufferLayer eMRDBufLyr)
	{
		long lRtn = SMT_ERR_FAILURE;

		lRect lrt;

		LRectToDRect(frect,lrt);

		switch (eMRDBufLyr)
		{
		case MRD_BL_MAP:
			{
				lRtn = m_smtMapRenderBuf.StrethImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.width(),lrt.height());
			}
			break;

		case MRD_BL_DYNAMIC:
			{
				lRtn = m_smtDynamicRenderBuf.StrethImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.width(),lrt.height());
			}
			break;

		case MRD_BL_QUICK:
			{
				lRtn = m_smtQuickRenderBuf.StrethImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.width(),lrt.height());
			}
			break;
		}

		return lRtn;
	}

	int SmtGdiRenderDevice::SaveImage(const char * szFilePath,eRDBufferLayer eMRDBufLyr,bool bBgTransparent)
	{
		long lRtn = SMT_ERR_FAILURE;

		switch (eMRDBufLyr)
		{
		case MRD_BL_MAP:
			{
				lRtn = m_smtMapRenderBuf.Save2Image(szFilePath,bBgTransparent);
			}
			break;

		case MRD_BL_DYNAMIC:
			{
				lRtn = m_smtDynamicRenderBuf.Save2Image(szFilePath,bBgTransparent);
			}
			break;

		case MRD_BL_QUICK:
			{
				lRtn = m_smtQuickRenderBuf.Save2Image(szFilePath,bBgTransparent);
			}
			break;
		}

		return lRtn;
	}

	int SmtGdiRenderDevice::Save2ImageBuf(char *&szImageBuf,long &lImageBufSize,long lCodeType,eRDBufferLayer eMRDBufLyr,bool bBgTransparent)
	{
		long lRtn = SMT_ERR_FAILURE;

		switch (eMRDBufLyr)
		{
		case MRD_BL_MAP:
			{
				lRtn = m_smtMapRenderBuf.Save2ImageBuf(szImageBuf,lImageBufSize,lCodeType,bBgTransparent);
			}
			break;
		case MRD_BL_DYNAMIC:
			{
				lRtn = m_smtDynamicRenderBuf.Save2ImageBuf(szImageBuf,lImageBufSize,lCodeType,bBgTransparent);
			}
			break;
		case MRD_BL_QUICK:
			{
				lRtn = m_smtQuickRenderBuf.Save2ImageBuf(szImageBuf,lImageBufSize,lCodeType,bBgTransparent);
			}
			break;
		}

		return lRtn;
	}

	int SmtGdiRenderDevice::FreeImageBuf(char *&szImageBuf)
	{
		return SmtRenderBuf::FreeImageBuf(szImageBuf);
	}
}
