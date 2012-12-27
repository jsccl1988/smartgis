#include <math.h>

#include "gdi_renderdevice.h"
#include "smt_logmanager.h"
#include "bl_api.h"
#include "gdi_aux_api.h"
#include "Resource.h"
#include "gdi_renderthread.h"
#include "smt_api.h"
#include "geo_api.h"
#include "ximage.h"

using namespace Smt_GIS;
using namespace Smt_Core;
using namespace Smt_Geo;

namespace Smt_Rd
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

	   SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
	   SmtLog *pLog = pLogMgr->CreateLog(logname);

	   if (NULL == pLog)
		   return SMT_ERR_FAILURE;

	   pLog->LogMessage("Init Gdi SmtRenderDevice ok!");
	 
	   m_strLogName = logname;

	   m_smtRenderBuf.SetWnd(m_hWnd);
	   m_smtMapRenderBuf.SetWnd(m_hWnd);
	   m_smtDynamicRenderBuf.SetWnd(m_hWnd);
	   m_smtQuickRenderBuf.SetWnd(m_hWnd);
	   
	   m_pRenderThread->Init(m_hWnd,logname);
	   m_pRenderThread->Start();
	   m_pRenderThread->Resume();
	 //  m_pRenderThread->Suspend();

	   return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::Destroy(void)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(m_strLogName.c_str());
		if (pLog != NULL)
			pLog->LogMessage("Destroy Gdi SmtRenderDevice ok!");

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::Release(void)
	{
	   SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
	   SmtLog *pLog = pLogMgr->GetLog(m_strLogName.c_str());
	   if (pLog != NULL)
		   pLog->LogMessage("Release Gdi SmtRenderDevice ok!");

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
		 
		if (IsEqual(m_Viewport.m_fVOX,orgx,dEPSILON) && 
			IsEqual(m_Viewport.m_fVOY,orgy,dEPSILON) &&
			IsEqual(m_Viewport.m_fVHeight,cy,dEPSILON) &&
			IsEqual(m_Viewport.m_fVWidth,cx,dEPSILON) )
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
		if (IsEqual(m_Windowport.m_fWWidth,0,dEPSILON) && 
			IsEqual(m_Windowport.m_fWHeight,0,dEPSILON) &&
			IsEqual(m_Viewport.m_fVWidth,0,dEPSILON) &&
			IsEqual(m_Viewport.m_fVHeight,0,dEPSILON) )
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
		if (IsEqual(m_Windowport.m_fWWidth,0,dEPSILON) && 
			IsEqual(m_Windowport.m_fWHeight,0,dEPSILON) &&
			IsEqual(m_Viewport.m_fVWidth,0,dEPSILON) &&
			IsEqual(m_Viewport.m_fVHeight,0,dEPSILON) )
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
		m_cslock.Lock();
		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::Unlock()
	{
		m_cslock.Unlock();
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
		ClearRect(hDC,invalidatex1, invalidatey1, invalidatew1, invalidateh1/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);
		ClearRect(hDC,invalidatex2, invalidatey2, invalidatew2, invalidateh2/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

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
		{//改变m_virViewport1
			m_virViewport1.m_fVHeight /= fscale;
			m_virViewport1.m_fVWidth  /= fscale;
			m_virViewport1.m_fVOX = orgPoint.x - (orgPoint.x-m_virViewport1.m_fVOX)/fscale;
			m_virViewport1.m_fVOY = orgPoint.y - (orgPoint.y-m_virViewport1.m_fVOY)/fscale;
		}
		else
		{//改变m_virViewport2
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
		m_virViewport2.m_fVHeight = rt.Height();
		m_virViewport2.m_fVWidth = rt.Width();
		
		m_Windowport.m_fWOX = rect.lb.x;
		m_Windowport.m_fWOY = rect.lb.y;
		m_Windowport.m_fWHeight = rect.Height();
		m_Windowport.m_fWWidth  = rect.Width(); 

		float xblc,yblc;
		xblc = m_Viewport.m_fVWidth/m_Windowport.m_fWWidth;
		yblc = m_Viewport.m_fVHeight/m_Windowport.m_fWHeight;

		m_fblc = (xblc > yblc)?yblc:xblc;

		if (xblc < yblc)
		{
			m_virViewport2.m_fVOY += rt.Height()*(1-yblc/xblc);
			m_Windowport.m_fWHeight = rect.Height()*yblc/xblc;
			m_virViewport2.m_fVHeight = rt.Height()*yblc/xblc;
		}
		else
		{
			m_Windowport.m_fWWidth  = rect.Width()*xblc/yblc; 
			m_virViewport2.m_fVWidth = rt.Width()*xblc/yblc;
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
			{//激活重绘
				m_bRedraw = false;
				m_pRenderThread->Resume();							//启动绘图线程
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
			ulong format = pStyle->GetStyleType();
			if( format & ST_PenDesc )
			{
				SmtPenDesc pen = pStyle->GetPenDesc();

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
				SmtBrushDesc brush = pStyle->GetBrushDesc();

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
				SmtSymbolDesc symbol = pStyle->GetSymbolDesc();

				if (m_hIcon)
				{
					DeleteObject(m_hIcon);
					m_hIcon = NULL;
				}

				m_hIcon = LoadIcon(m_hInst,MAKEINTRESOURCE(symbol.lSymbolID+IDI_ICON_A));
			}

			if (format & ST_AnnoDesc)
			{
				SmtAnnotationDesc anno = pStyle->GetAnnoDesc();

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

		pMap->MoveFirst();
		while (!pMap->IsEnd())
		{
			RenderLayer(pMap->GetLayer(),op);
			pMap->MoveNext();
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::RenderLayer(const SmtLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		if(pLayer->GetLayerType() == LYR_VECTOR)
			return RenderLayer((SmtVectorLayer*)pLayer,op);
		else if(pLayer->GetLayerType() == LYR_RASTER)
			return RenderLayer((SmtRasterLayer*)pLayer,op);
		else if(pLayer->GetLayerType() == LYR_TITLE)
			return RenderLayer((SmtTileLayer*)pLayer,op);

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderDevice::RenderLayer(const SmtVectorLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		if (!pLayer->IsVisible())
			return SMT_ERR_NONE; 

		Envelope envLayer ;
		pLayer->GetEnvelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;

		ViewportToRect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);

		if (!envLayer.Intersects(envViewp))
			return SMT_ERR_NONE;
	
		pLayer->MoveFirst();
		while (!pLayer->IsEnd())
		{
			RenderFeature(pLayer->GetFeature(),op);
			pLayer->MoveNext();
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
		pLayer->GetEnvelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;

		ViewportToRect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);

		if (!envLayer.Intersects(envViewp))
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
			tmpImage.Stretch(m_hCurDC,lrt.lb.x,lrt.rt.y,lrt.Width(),lrt.Height());
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
		pLayer->GetEnvelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;
		lRect titleDPRect;

		ViewportToRect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);

		if (!envLayer.Intersects(envViewp))
			return SMT_ERR_NONE;

		pLayer->MoveFirst();
		while (!pLayer->IsEnd())
		{
			SmtTile *pTile = pLayer->GetTile();
			if (NULL != pTile && pTile->bVisible)
			{
			/*	Envelope envTile,envViewp;
				RectToEnvelope(envTile,pTile->rtTileRect);
				LRectToDRect(pTile->rtTileRect,titleDPRect);

				if (!envTile.Intersects(envViewp) ||
					(titleDPRect.Height() < 2 && titleDPRect.Width() < 2))
					return SMT_ERR_NONE;*/

				CxImage tmpImage;
				tmpImage.Decode((BYTE*)pTile->pTileBuf,pTile->lTileBufSize,pTile->lImageCode);
				tmpImage.Stretch(m_hCurDC,titleDPRect.lb.x,titleDPRect.rt.y,titleDPRect.Width(),titleDPRect.Height());
			}			

			pLayer->MoveNext();
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::RenderFeature(const SmtFeature *pFeature,int op)
	{
		if (NULL == pFeature)
			return SMT_ERR_INVALID_PARAM;

        const SmtStyle		*pStyle = pFeature->GetStyle();
		const SmtGeometry	*pGeom  = pFeature->GetGeometryRef();
		const SmtAttribute	*pAtt = pFeature->GetAttributeRef();

		m_nFeatureType = pFeature->GetFeatureType();
		
		switch(m_nFeatureType)
		{
		case SmtFeatureType::SmtFtAnno:
			{
				const  SmtField *pFld = NULL;

				pFld = pAtt->GetFieldPtr(pAtt->GetFieldIndex("anno"));
				sprintf(m_szAnno,pFld->GetValueAsString());

				pFld = pAtt->GetFieldPtr(pAtt->GetFieldIndex("angle"));
				m_fAnnoAngle = pFld->GetValueAsDouble();
			}
			break;
		case SmtFeatureType::SmtFtChildImage:
			{
				 
			}
		    break;
		case SmtFeatureType::SmtFtSurface:
			{
              // nDrawMode = R2_MASKPEN;
			}
			break;
		default:
		    break;
		}
		
		return RenderGeometry(pGeom,pStyle,op);	
	}

	int SmtGdiRenderDevice::RenderGeometry(const SmtGeometry *pGeom,const SmtStyle*pStyle,int op)
	{
		if (!pGeom)
			return SMT_ERR_INVALID_PARAM;

		SmtGeometryType type  = pGeom->GetGeometryType();

		Envelope envFeature,envViewp;
		pGeom->GetEnvelope(&envFeature);

		lRect lViewp;
		fRect fViewp;
		fRect fenv;
		lRect lenv;

		ViewportToRect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);

		EnvelopeToRect(fenv,envFeature);
		LRectToDRect(fenv,lenv);

		if (!envFeature.Intersects(envViewp) ||
			(type !=GTPoint && lenv.Height() < 2 && lenv.Width() < 2))
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

		switch(type)
		{
		case GTPoint:
			DrawPoint(pStyle,(SmtPoint*)pGeom);
			break;

		case GTLineString:
			DrawLineString((SmtLineString*)pGeom);
			break;
		case GTArc:
			DrawArc((SmtArc*)pGeom);
			break;

		case GTPolygon:
			DrawPloygon((SmtPolygon*)pGeom);
			break;

		case GTFan:
            DrawFan((SmtFan*)pGeom);
			break;

		case GTMultiPoint:
			DrawMultiPoint(pStyle,(SmtMultiPoint*)pGeom);
			break;

		case GTMultiLineString:
			DrawMultiLineString((SmtMultiLineString*)pGeom);
			break;

		case GTMultiPolygon:
			DrawMultiPolygon((SmtMultiPolygon*)pGeom);
			break;

		case GTSpline:
			DrawLineSpline((SmtSpline*)pGeom);
			break;

		case GTLinearRing:
			DrawLinearRing((SmtLinearRing*)pGeom);
			break;

		case GTGrid:
			DrawGrid((SmtGrid*)pGeom);
			break;

		case GTTin:
			DrawTin((SmtTin*)pGeom);
			break;

		case GTNone:
		case GTUnknown:
		default:
			break;
		}

		if(!m_bLockStyle)
			EndDrawing();

		::RestoreDC(m_hCurDC,-1);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::DrawMultiLineString(const SmtMultiLineString *pMultiLinestring)
	{
		int nLines = pMultiLinestring->GetNumGeometries();

		int i = 0;
		while (i < nLines)
		{
			DrawLineString((SmtLineString*)pMultiLinestring->GetGeometryRef(i));
			i++;
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawMultiPoint(const SmtStyle*pStyle,const SmtMultiPoint *pMultiPoint)
	{
		int nPoints = pMultiPoint->GetNumGeometries();

		int i = 0;
		while (i < nPoints)
		{
			DrawPoint(pStyle,(SmtPoint*)pMultiPoint->GetGeometryRef(i));
			i++;
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawMultiPolygon(const SmtMultiPolygon *pMultiPolygon)
	{
		int nPolygons = pMultiPolygon->GetNumGeometries();

		int i = 0;
		while (i < nPolygons)
		{
			DrawPloygon((SmtPolygon*)pMultiPolygon->GetGeometryRef(i));
			i++;
		}

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::DrawPoint(const SmtStyle*pStyle,const SmtPoint *pPoint)
	{
		ulong format = pStyle->GetStyleType();
		if (m_nFeatureType == SmtFeatureType::SmtFtAnno)
		{
			SmtAnnotationDesc anno = pStyle->GetAnnoDesc();
			return DrawAnno(m_szAnno,m_fAnnoAngle,abs(anno.fHeight),abs(anno.fWidth),abs(anno.fSpace),pPoint);
		}
		else if(m_nFeatureType == SmtFeatureType::SmtFtChildImage)
		{
			SmtSymbolDesc symbol = pStyle->GetSymbolDesc();
			return DrawSymbol(m_hIcon,symbol.fSymbolHeight,symbol.fSymbolWidth,pPoint);
		}
		else if (m_nFeatureType == SmtFeatureType::SmtFtDot)
		{
			int r = m_rdPra.lPointRaduis/**m_fblc*/;
			long lX,lY;
			LPToDP(pPoint->GetX(),pPoint->GetY(),lX,lY);
			Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);

			if (m_rdPra.bShowPoint)
			{
				//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
				DrawCross(m_hCurDC,lX,lY,r);
			}

			return SMT_ERR_NONE;
		}	

		return SMT_ERR_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderDevice::DrawAnno(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const SmtPoint *pPoint)
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

		LPToDP(pPoint->GetX(),pPoint->GetY(),x,y);
		pt.x = x;
		pt.y = y;

		pt.x -= 2*fCHeight*sin(fangel);
		pt.y -= 2*fCHeight*cos(fangel);

		int nStrLength  = (int)strlen(ls1);
		while(nStrLength > 0)
		{
			c1 = *ls1;
			c2 = *(ls1 + 1);
			if(c1 >127 && c2 > 127) //如果下一个字符是汉字
			{
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
			LPToDP(pPoint->GetX(),pPoint->GetY(),lX,lY);
			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			DrawCross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawSymbol(HICON hIcon,long lHeight,long lWidth,const SmtPoint *pPoint)
	{
		lHeight *= m_fblc;
		lWidth *= m_fblc;

		long lX,lY;
		LPToDP(pPoint->GetX(),pPoint->GetY(),lX,lY);
		//::DrawIcon(m_hCurDC,pt.x-lWidth,pt.y-lHeight,hIcon);
		::DrawIconEx(m_hCurDC,lX-lWidth/2,lY+lHeight/2,   hIcon, lWidth, lHeight, 0, NULL, DI_NORMAL);
		//::DrawState(m_hCurDC,NULL,NULL,(LPARAM)hIcon,0,pt.x-lWidth/2,pt.y+lHeight/2,lWidth,lHeight, DSS_NORMAL | DST_ICON);

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			long lX,lY;
			LPToDP(pPoint->GetX(),pPoint->GetY(),lX,lY);
			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			DrawCross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawLineSpline(const SmtSpline *pSpline)
	{
		int    nPoints = pSpline->GetAnalyticPointCount();
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
				LPToDP(pSpline->GetAnalyticX(i),pSpline->GetAnalyticY(i),lpPoint[i].x,lpPoint[i].y);
				DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}

			MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
			PolylineTo(m_hCurDC, lpPoint, nPoints);
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pSpline->GetAnalyticX(i),pSpline->GetAnalyticY(i),lpPoint[i].x,lpPoint[i].y);
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
		for (int i = 0; i < pSpline->GetNumPoints();i++)
		{
			LPToDP(pSpline->GetX(i),pSpline->GetY(i),lX,lY);
			Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawLineString(const SmtLineString *pLinestring)
	{
		int    nPoints = pLinestring->GetNumPoints();
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
				LPToDP(pLinestring->GetX(i),pLinestring->GetY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinestring->GetX(i),pLinestring->GetY(i),lpPoint[i].x,lpPoint[i].y);
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

	int SmtGdiRenderDevice::DrawLinearRing(const SmtLinearRing *pLinearRing)
	{
		int    nPoints = pLinearRing->GetNumPoints();
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
			for (int i = 0; i < pLinearRing->GetNumPoints();i++)
			{
				LPToDP(pLinearRing->GetX(i),pLinearRing->GetY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinearRing->GetX(i),pLinearRing->GetY(i),lpPoint[i].x,lpPoint[i].y);
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

	int SmtGdiRenderDevice::DrawPloygon(const SmtPolygon *pPloygon)
	{
		int nAllPts = 0;
		const SmtLinearRing *pLinerring = pPloygon->GetExteriorRing();

		int    nExteriorPts = pLinerring->GetNumPoints();
		if (nExteriorPts < 2)
			return SMT_ERR_INVALID_PARAM;

		BOOL bRet = FALSE;

		nAllPts += nExteriorPts;

		int nInteriorRings = pPloygon->GetNumInteriorRings();
		int *nRings = new int[nInteriorRings + 1];
		nRings[0] = nExteriorPts;

		for (int i = 0; i < nInteriorRings ; i++)
		{
			const SmtLinearRing *pInteriorRing = pPloygon->GetInteriorRing(i);
			nRings[i+1] = pInteriorRing->GetNumPoints();
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
				LPToDP(pLinerring->GetX(i),pLinerring->GetY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}

			for (int i = 0; i < nInteriorRings ;i++)
			{
				const SmtLinearRing *pInteriorRing = pPloygon->GetInteriorRing(i);
				int nInteriorPts= pInteriorRing->GetNumPoints();
				for ( int j=0; j<nInteriorPts; ++j,nCount++)
				{
					LPToDP(pInteriorRing->GetX(i),pInteriorRing->GetY(i),lpPoint[i].x,lpPoint[i].y);
					//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
				}
			}

			bRet = ::PolyPolygon(m_hCurDC,lpPoint,nRings,nInteriorRings+1);
		}
		else
		{
			for (int i = 0;i < nExteriorPts ; i++,nCount++)
			{
				LPToDP(pLinerring->GetX(i),pLinerring->GetY(i),lpPoint[i].x,lpPoint[i].y);
			}

			for (int i = 0; i < nInteriorRings ;i++)
			{
				const SmtLinearRing *pInteriorRing = pPloygon->GetInteriorRing(i);
				int nInteriorPts= pInteriorRing->GetNumPoints();
				for ( int j=0; j<nInteriorPts; ++j,nCount++)
				{
					LPToDP(pInteriorRing->GetX(i),pInteriorRing->GetY(i),lpPoint[i].x,lpPoint[i].y);
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

	//绘制Tin线
	int SmtGdiRenderDevice::DrawTinLines(const SmtTin *pTin)
	{
		POINT	 lPt1,lPt2,lPt3;
		SmtPoint oPt1,oPt2,oPt3;

		Envelope envTri,envViewp;
		lRect lViewp;
		fRect fViewp;

		ViewportToRect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);
	
		for (int i = 0; i < pTin->GetTriangleCount();i++)
		{
			SmtTriangle tri = pTin->GetTriangle(i);

			if (!tri.bDelete)
			{
				oPt1 = pTin->GetPoint(tri.a);
				oPt2 = pTin->GetPoint(tri.b);
				oPt3 = pTin->GetPoint(tri.c);

				envTri.Merge(oPt1.GetX(),oPt1.GetY());
				envTri.Merge(oPt2.GetX(),oPt2.GetY());
				envTri.Merge(oPt3.GetX(),oPt3.GetY());

				if (envTri.Intersects(envViewp))
				{
					LPToDP(oPt1.GetX(),oPt1.GetY(),lPt1.x,lPt1.y);
					LPToDP(oPt2.GetX(),oPt2.GetY(),lPt2.x,lPt2.y);
					LPToDP(oPt3.GetX(),oPt3.GetY(),lPt3.x,lPt3.y);

					MoveToEx(m_hCurDC,lPt1.x, lPt1.y, NULL);
					LineTo(m_hCurDC,lPt2.x,lPt2.y);
					LineTo(m_hCurDC,lPt3.x,lPt3.y);
					LineTo(m_hCurDC,lPt1.x,lPt1.y);
				}
			}
		}

		return SMT_ERR_NONE;
	}

	//绘制Tin节点
	int SmtGdiRenderDevice::DrawTinNodes(const SmtTin *pTin)
	{
		POINT		lPt;
		SmtPoint	oPt;
		int			r = m_rdPra.lPointRaduis;

		lRect lViewp;
		fRect fViewp;

		ViewportToRect(lViewp,m_Viewport);
		DRectToLRect(lViewp,fViewp);
		AjustfRect(fViewp);

		for (int i = 0; i < pTin->GetPointCount(); i++)
		{
			oPt = pTin->GetPoint(i);
			if (IsInfRect(oPt.GetX(),oPt.GetY(),fViewp) )
			{
				LPToDP(oPt.GetX(),oPt.GetY(),lPt.x,lPt.y);
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

	//绘制网格线
	int SmtGdiRenderDevice::DrawGridLines(const SmtGrid *pGrid)
	{
		const Matrix2D<RawPoint>  *pNodes = pGrid->GetGridNodeBuf();

		int nM,nN;
		pGrid->GetSize(nM,nN);

		POINT lPt;

		for (int j = 0; j < nN; j ++)
		{//绘制列
			RawPoint rawPt = pNodes->GetElement(0,j);
			LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
			MoveToEx(m_hCurDC,lPt.x, lPt.y, NULL);
			for (int i = 0; i < nM; i++)
			{//绘制行
				RawPoint rawPt1 = pNodes->GetElement(i,j);
				LPToDP(rawPt1.x,rawPt1.y,lPt.x,lPt.y);
				LineTo(m_hCurDC,lPt.x,lPt.y);
			}	 
		}

		for (int i = 0; i < nM ; i ++)
		{//绘制列
			RawPoint rawPt = pNodes->GetElement(i,0);
			LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
			MoveToEx(m_hCurDC,lPt.x, lPt.y, NULL);

			for (int j = 0; j < nN ;j ++)
			{//绘制行
				RawPoint rawPt1 = pNodes->GetElement(i,j);
				LPToDP(rawPt1.x,rawPt1.y,lPt.x,lPt.y);
				LineTo(m_hCurDC,lPt.x,lPt.y);
			}
		}

		return SMT_ERR_NONE;
	}

	//绘制网格节点
	int SmtGdiRenderDevice::DrawGridNodes(const SmtGrid *pGrid)
	{
		const Matrix2D<RawPoint>  *pNodes = pGrid->GetGridNodeBuf();

		int nM,nN;
		pGrid->GetSize(nM,nN);

		int r = m_rdPra.lPointRaduis;
		POINT lPt;

		for (int j = 0; j < nN; j ++)
		{
			for (int i = 0; i < nM; i++)
			{
				RawPoint rawPt = pNodes->GetElement(i,j);
				LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
				Ellipse(m_hCurDC,lPt.x - r ,lPt.y - r,lPt.x + r ,lPt.y + r);
			}
		}

		return SMT_ERR_NONE;
	}

    int  SmtGdiRenderDevice::DrawFan(const SmtFan *pFan)
	{
		long x1,y1,x2,y2,x3,y3,x4,y4,x5,y5;
		SmtPoint oStPoint,oEdbfPoint,oCtPoint;

		const  SmtArc *pArc = pFan->GetArc();
		pArc->StartPoint(&oStPoint);
		pArc->EndPoint(&oEdbfPoint);
		pArc->GetCenterPoint(&oCtPoint);

		LPToDP(oStPoint.GetX(),oStPoint.GetY(),x3,y3);
		LPToDP(oEdbfPoint.GetX(),oEdbfPoint.GetY(),x4,y4);
		LPToDP(oCtPoint.GetX(),oCtPoint.GetY(),x5,y5);

		int dr = m_rdPra.lPointRaduis;
		float r = SmtDistance(x4,y4,x5,y5);

		x1 = x5 - r;
		y1 = y5 - r;
		x2 = x5 + r;
		y2 = y5 + r;

		::MoveToEx(m_hCurDC,x4,y4,NULL);
		LineTo(m_hCurDC,x5,y5);
		LineTo(m_hCurDC,x3,y3);
		::Pie(m_hCurDC,x1,y1,x2,y2,x3,y3,x4,y4);

		Ellipse(m_hCurDC,x3 - dr ,y3 - dr,x3 + dr ,y3 + dr);
		Ellipse(m_hCurDC,x4 - dr ,y4 - dr,x4 + dr ,y4 + dr);
		Ellipse(m_hCurDC,x5 - dr ,y5 - dr,x5 + dr ,y5 + dr);

		if (m_rdPra.bShowPoint)
		{
			//Rectangle(m_hCurDC,x3 - dr ,y3 - dr,x3 + dr ,y3 + dr);
			//Rectangle(m_hCurDC,x4 - dr ,y4 - dr,x4 + dr ,y4 + dr);
			//Rectangle(m_hCurDC,x5 - dr ,y5 - dr,x5 + dr ,y5 + dr);

			DrawCross(m_hCurDC,x3,y3,dr);
			DrawCross(m_hCurDC,x4,y4,dr);
			DrawCross(m_hCurDC,x5,y5,dr);
		}

		return SMT_ERR_NONE;
	}

	int  SmtGdiRenderDevice::DrawArc(const SmtArc *pArc)
	{
		long x1,y1,x2,y2,x3,y3,x4,y4,x5,y5;
        SmtPoint oStPoint,oEdbfPoint,oCtPoint;

		pArc->StartPoint(&oStPoint);
		pArc->EndPoint(&oEdbfPoint);
		pArc->GetCenterPoint(&oCtPoint);

		LPToDP(oStPoint.GetX(),oStPoint.GetY(),x3,y3);
		LPToDP(oEdbfPoint.GetX(),oEdbfPoint.GetY(),x4,y4);
		LPToDP(oCtPoint.GetX(),oCtPoint.GetY(),x5,y5);

		int dr = m_rdPra.lPointRaduis;
		float r = SmtDistance(x4,y4,x5,y5);

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

			DrawCross(m_hCurDC,x3,y3,dr);
			DrawCross(m_hCurDC,x4,y4,dr);
			DrawCross(m_hCurDC,x5,y5,dr);
		}

		return SMT_ERR_NONE;
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
			fRectTolRect(lrect,frect);

		Ellipse(m_hCurDC,lrect.lb.x ,lrect.lb.y,lrect.rt.x ,lrect.rt.y);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderDevice::DrawRect(const fRect &rect,bool bDP )
	{
		lRect tmpRectDP;

		fRectTolRect(tmpRectDP,rect);

		if (!bDP)
		{
			fRect tmpRectLP;

			lRectTofRect(tmpRectLP,tmpRectDP);
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
					DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
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
					DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
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
			if(c1 >127 && c2 > 127) //如果下一个字符是汉字
			{
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
			DrawCross(m_hCurDC,lX,lY,r);
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
				lRtn = m_smtMapRenderBuf.DrawImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.Width(),lrt.Height());
			}
			break;

		case MRD_BL_DYNAMIC:
			{
				lRtn = m_smtDynamicRenderBuf.DrawImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.Width(),lrt.Height());
			}
			break;

		case MRD_BL_QUICK:
			{
				lRtn = m_smtQuickRenderBuf.DrawImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.Width(),lrt.Height());
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
				lRtn = m_smtMapRenderBuf.StrethImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.Width(),lrt.Height());
			}
			break;

		case MRD_BL_DYNAMIC:
			{
				lRtn = m_smtDynamicRenderBuf.StrethImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.Width(),lrt.Height());
			}
			break;

		case MRD_BL_QUICK:
			{
				lRtn = m_smtQuickRenderBuf.StrethImage(szImageBuf,nImageBufSize,lCodeType,lrt.lb.x,lrt.rt.y,lrt.Width(),lrt.Height());
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