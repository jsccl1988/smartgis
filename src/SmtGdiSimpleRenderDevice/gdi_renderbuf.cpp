#include "gdi_renderbuf.h"
#include "ximage.h"
#include "smt_api.h"

#pragma   comment(lib,"Msimg32.lib")//TransparentBlt¿â

namespace Smt_Rd
{
	SmtRenderBuf::SmtRenderBuf(HWND hWnd):m_hWnd(hWnd)
		,m_bOnwerBuf(true)
	{

	}

	SmtRenderBuf::SmtRenderBuf(void):m_hWnd(NULL)
		,m_bOnwerBuf(true)
	{

	}

	SmtRenderBuf::~SmtRenderBuf(void)
	{
		if (m_bOnwerBuf && m_hPaintBuf)
		{
			DeleteObject(m_hPaintBuf);	 
			m_hPaintBuf = NULL;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	inline long SmtRenderBuf::SetWnd(HWND hWnd) 
	{
		if (!m_bOnwerBuf)
			return SMT_ERR_FAILURE;

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		m_hWnd = hWnd;
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return SMT_ERR_NONE;
	}

	long SmtRenderBuf::SetBufSize(int cx,int cy)
	{
		if (!m_bOnwerBuf || cx < 1 || cy < 1)
			return SMT_ERR_FAILURE;

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		if (m_hPaintBuf)
		{
			DeleteObject(m_hPaintBuf);	 
			m_hPaintBuf = NULL;
		}

		HDC hDC = GetDC(m_hWnd);	
		m_nBufWidth   = cx;
		m_nBufHeight  = cy;
		m_hPaintBuf   = CreateCompatibleBitmap(hDC,m_nBufWidth,m_nBufHeight); 

		if (NULL == m_hPaintBuf)
			return SMT_ERR_FAILURE;

		ClearBuf(0,0,m_nBufWidth,m_nBufHeight/*,(COLORREF)::GetSysColor(COLOR_WINDOW)*/);

		::ReleaseDC(m_hWnd,hDC);
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		return SMT_ERR_NONE;
	}

	long SmtRenderBuf::ShareBuf(SmtRenderBuf &rbSrc)
	{
		if (m_bOnwerBuf && m_hPaintBuf)
		{
			DeleteObject(m_hPaintBuf);	 
			m_hPaintBuf = NULL;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		this->SetWnd(rbSrc.GetWnd());
		this->m_nBufHeight = rbSrc.GetBufHeight();
		this->m_nBufWidth = rbSrc.GetBufWidth();
		this->m_hPaintBuf = rbSrc.GetBuf();

		m_bOnwerBuf = false;

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		return SMT_ERR_NONE;
	}

	long SmtRenderBuf::ClearBuf(int x,int y,int w,int h,COLORREF clr)
	{
		if (w < 1 || h< 1)
			return SMT_ERR_FAILURE;

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		RECT rect;
		rect.left   = x;
		rect.bottom = y;
		rect.right  = x + w;
		rect.top    = y + h;

		HDC hDC = GetDC(m_hWnd);
		HDC hPaintBufDC = CreateCompatibleDC(hDC);
		HBITMAP hPrePaintBuf = (HBITMAP)::SelectObject(hPaintBufDC,m_hPaintBuf); 

		HBRUSH hBrush = CreateSolidBrush(clr);
		HBRUSH hOldBrush = (HBRUSH)::SelectObject(hPaintBufDC,hBrush);

		::FillRect(hPaintBufDC,&rect,hBrush);

		::SelectObject(hPaintBufDC,hOldBrush);
		::DeleteObject(hBrush);

		::SelectObject(hPaintBufDC,hPrePaintBuf); 
		::DeleteObject(hPaintBufDC);	

		::ReleaseDC(m_hWnd,hDC);
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		return SMT_ERR_NONE;
	}

	long SmtRenderBuf::SwapBuf(int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int op)
	{
		if ( m_hPaintBuf == NULL )  
			return SMT_ERR_FAILURE;

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		HDC hDC = GetDC(m_hWnd);
		HDC hSrcDC = this->PrepareDC(false);

		BOOL bRet = ::BitBlt(hDC,destOrgx,destOrgy,destW,destH,hSrcDC,srcOrgx,srcOrgy,op);

		this->EndDC();
		::ReleaseDC(m_hWnd,hDC);
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		if (bRet)
			return SMT_ERR_NONE;
		else
			return SMT_ERR_FAILURE;
	}

	long SmtRenderBuf::SwapBuf(int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int srcW,int srcH,eSwapType type,int op,COLORREF clr)
	{
		if ( m_hPaintBuf == NULL )  
			return SMT_ERR_FAILURE;

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		HDC hDC = GetDC(m_hWnd);
		HDC hSrcDC = this->PrepareDC(false);
		BOOL bRet = TRUE;

		switch (type)
		{
		case BLT_STRETCH:
			bRet = ::StretchBlt(hDC,destOrgx,destOrgy,destW,destH,hSrcDC,srcOrgx,srcOrgy,srcW,srcH,op);
			break;
		case BLT_TRANSPARENT:
			bRet = ::TransparentBlt(hDC,destOrgx,destOrgy,destW,destH,hSrcDC,srcOrgx,srcOrgy,destW,destH,clr);
			break;
		}

		this->EndDC();
		::ReleaseDC(m_hWnd,hDC);

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		if (bRet)
			return SMT_ERR_NONE;
		else
			return SMT_ERR_FAILURE;
	}

	long SmtRenderBuf::SwapBuf(SmtRenderBuf &rbTarget,int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int op)
	{
		if ( m_hPaintBuf == NULL )  
			return SMT_ERR_FAILURE;

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		HDC hTargetDC = rbTarget.PrepareDC(false);
		HDC hSrcDC = this->PrepareDC(false);

		BOOL bRet = ::BitBlt(hTargetDC,destOrgx,destOrgy,destW,destH,hSrcDC,srcOrgx,srcOrgy,op);

		this->EndDC();
		rbTarget.EndDC();

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		if (bRet)
			return SMT_ERR_NONE;
		else
			return SMT_ERR_FAILURE;
	}

	long SmtRenderBuf::SwapBuf(SmtRenderBuf &rbTarget,int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int srcW,int srcH,eSwapType type,int op,COLORREF clr)
	{
		if ( m_hPaintBuf == NULL )  
			return SMT_ERR_FAILURE;

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		HDC hTargetDC = rbTarget.PrepareDC(false);
		HDC hSrcDC = this->PrepareDC(false);

		BOOL bRet = TRUE;

		switch (type)
		{
		case BLT_STRETCH:
			bRet = ::StretchBlt(hTargetDC,destOrgx,destOrgy,destW,destH,hSrcDC,srcOrgx,srcOrgy,srcW,srcH,op);
			break;
		case BLT_TRANSPARENT:
			bRet = ::TransparentBlt(hTargetDC,destOrgx,destOrgy,destW,destH,hSrcDC,srcOrgx,srcOrgy,srcW,srcH,clr);
			break;
		}

		this->EndDC();
		rbTarget.EndDC();
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		if (bRet)
			return SMT_ERR_NONE;
		else
			return SMT_ERR_FAILURE;
	}

	HDC SmtRenderBuf::PrepareDC(bool bClip)
	{
		HDC hDC = GetDC(m_hWnd);
		m_hPaintDC = CreateCompatibleDC( hDC );
		::ReleaseDC(m_hWnd,hDC);

		m_hOldPaintBuf = (HBITMAP)SelectObject(m_hPaintDC,m_hPaintBuf);

		if (bClip)
		{
			RECT rtClip;
			HRGN hNewRgn = ::CreateRectRgn(0,0,m_nBufWidth,m_nBufHeight);
			::GetClipBox(m_hPaintDC,&rtClip) ;
			::SelectClipRgn(m_hPaintDC,hNewRgn);
			::DeleteObject(hNewRgn);	
		}

		return m_hPaintDC;
	}

	long SmtRenderBuf::EndDC(void)
	{
		if (m_hPaintDC)
		{
			SelectObject(m_hPaintDC, m_hOldPaintBuf );
			DeleteDC(m_hPaintDC);

			m_hPaintDC = NULL;
		}

		return SMT_ERR_NONE;
	}

	SmtRenderBuf & SmtRenderBuf::operator = (const SmtRenderBuf &other)
	{
		if (&other == this)
			return *this;

		if (m_bOnwerBuf && NULL != m_hPaintBuf)
		{
			DeleteObject(m_hPaintBuf);	 
			m_hPaintBuf = NULL;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		this->SetWnd(other.GetWnd());
		this->SetBufSize(other.GetBufWidth(),other.GetBufHeight());

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtRenderBuf::DrawImage(const char *szImageBuf,int nImageBufSize,long lCodeType,long x, long y, long cx, long cy)
	{
		if (NULL == szImageBuf || 0 == nImageBufSize)
			return SMT_ERR_INVALID_PARAM;

		CxImage tmpImage;
		tmpImage.Decode((BYTE*)szImageBuf,nImageBufSize,lCodeType);

		HDC hDC = PrepareDC();
		tmpImage.Draw(hDC,x, y,cx,cy);
		EndDC();

		return SMT_ERR_NONE;
	}

	long SmtRenderBuf::StrethImage(const char *szImageBuf,int nImageBufSize,long lCodeType,long xoffset, long yoffset, long xsize, long ysize, DWORD dwRop)
	{
		if (NULL == szImageBuf || 0 == nImageBufSize)
			return SMT_ERR_INVALID_PARAM;

		CxImage tmpImage;
		tmpImage.Decode((BYTE*)szImageBuf,nImageBufSize,lCodeType);

		HDC hDC = PrepareDC();
		tmpImage.Stretch(hDC, xoffset,yoffset,xsize,ysize,dwRop);
		EndDC();

		return SMT_ERR_NONE;
	}

	long SmtRenderBuf::Save2Image(const char * szFilePath,bool bBgTransparent)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		long lRtn =  Save2Image(m_hPaintBuf,szFilePath,bBgTransparent);

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		return lRtn;
	}

	long SmtRenderBuf::Save2ImageBuf(char *&szImageBuf,long &lImageBufSize,long lCodeType,bool bBgTransparent)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		long lRtn =  Save2ImageBuf(m_hPaintBuf,szImageBuf,lImageBufSize,lCodeType,bBgTransparent);

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		return lRtn;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtRenderBuf::Save2Image(HBITMAP hBitMap,const char * szFilePath,bool bBgTransparent)
	{
		if ( hBitMap == NULL ||strlen(szFilePath) == 0)  
			return SMT_ERR_FAILURE;

		CxImage image;

		if (image.CreateFromHBITMAP(hBitMap))
		{
			if (bBgTransparent)
			{
				COLORREF bgClr = RGB(255,255,255)/*(COLORREF)::GetSysColor(COLOR_WINDOW)*/;
				RGBQUAD  transClr;
				transClr.rgbRed = GetRValue(bgClr);
				transClr.rgbGreen = GetGValue(bgClr);
				transClr.rgbBlue = GetBValue(bgClr);
				transClr.rgbReserved = 0;

				image.SetTransIndex(0);
				image.SetTransColor(transClr);	
			}
			
			if (image.Save(szFilePath,GetImageTypeByFileExt(szFilePath)))
			{
				return SMT_ERR_NONE;
			}
		}

		return SMT_ERR_FAILURE;

	}

	long SmtRenderBuf::Save2ImageBuf(HBITMAP hBitMap,char *&szImageBuf,long &lImageBufSize,long lCodeType,bool bBgTransparent)
	{
		if ( hBitMap == NULL ||szImageBuf != NULL)  
			return SMT_ERR_FAILURE;

		BYTE *pImageBuf = NULL;
		long  lSize = 0;
		CxImage  image;

		if (image.CreateFromHBITMAP(hBitMap))
		{
			if (bBgTransparent)
			{
				COLORREF bgClr = RGB(255,255,255)/*(COLORREF)::GetSysColor(COLOR_WINDOW)*/;
				RGBQUAD  transClr;
				transClr.rgbRed = GetRValue(bgClr);
				transClr.rgbGreen = GetGValue(bgClr);
				transClr.rgbBlue = GetBValue(bgClr);
				transClr.rgbReserved = 0;

				image.SetTransIndex(0);
				image.SetTransColor(transClr);	
			}

			if (image.Encode(pImageBuf,lSize,lCodeType) )
			{
				szImageBuf = (char *)pImageBuf;
				lImageBufSize = lSize;

				return SMT_ERR_NONE;
			}
		
			return SMT_ERR_NONE;
		}
 
		return SMT_ERR_FAILURE;
	}

	long SmtRenderBuf::FreeImageBuf(char *&szImageBuf)
	{
		SMT_SAFE_DELETE_A(szImageBuf);

		return SMT_ERR_NONE;
	}

}