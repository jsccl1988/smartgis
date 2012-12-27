/*
File:    gdi_renderbuf.h

Desc:    SmtBufDC,GDI 缓冲DC 

Version: Version 1.0

Writter:  陈春亮

Date:    2011.11.16

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GDI_DC_BUF_H
#define _GDI_DC_BUF_H

#include "smt_core.h"
#include "smt_cslock.h"
using namespace  Smt_Core;
namespace Smt_Rd
{
	enum eSwapType
	{
		BLT_STRETCH,												//拉伸
		BLT_TRANSPARENT,											//透明
	};

	class SmtRenderBuf
	{
	public:
		SmtRenderBuf(void);
		SmtRenderBuf(HWND hWnd);
		~SmtRenderBuf(void);

	public:
		inline HWND		 GetWnd() const{return m_hWnd;}
		inline long		 SetWnd(HWND hWnd);

		long			 SetBufSize(int cx,int cy);
		inline long		 GetBufWidth(void) const {return m_nBufWidth;}
		inline long		 GetBufHeight(void) const {return m_nBufHeight;}

		inline HBITMAP	 GetBuf(void) {return m_hPaintBuf;}
		long			 ShareBuf(SmtRenderBuf &rbSrc);					//与另外一个buf共享同一个内存bmp，多线程使用有bug，可能与系统对象不能在多线程中使用有关

		long			 SwapBuf(int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int op = SRCCOPY);
		long			 SwapBuf(int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int srcW,int srcH,eSwapType type = BLT_TRANSPARENT,int op = SRCCOPY,COLORREF clr = RGB(255,255,255));//Transparent/Stretch
	
		long			 SwapBuf(SmtRenderBuf &rbTarget,int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int op = SRCCOPY);
		long			 SwapBuf(SmtRenderBuf &rbTarget,int destOrgx,int destOrgy,int destW,int destH,int srcOrgx,int srcOrgy,int srcW,int srcH,eSwapType type = BLT_TRANSPARENT,int op = SRCCOPY,COLORREF clr = RGB(255,255,255));//Transparent/Stretch
		
		HDC				 PrepareDC(bool bClip = true);					//准备dc，与EndDC()配对使用，之间代码段不能调用 ClearBuf()、SwapBuf()、SetBufSize()
		long			 EndDC(void);									//释放dc

		long			 ClearBuf(int x,int y,int w,int h,COLORREF clr = RGB(255,255,255));

		//////////////////////////////////////////////////////////////////////////
		long             DrawImage(const char *szImageBuf,int nImageBufSize,long lCodeType,long x=0, long y=0, long cx = -1, long cy = -1);
		long             StrethImage(const char *szImageBuf,int nImageBufSize,long lCodeType,long xoffset, long yoffset, long xsize, long ysize, DWORD dwRop = SRCCOPY);

		long			 Save2Image(const char * szFilePath,bool bBgTransparent = false );
		long			 Save2ImageBuf(char *&szImageBuf,long &lImageBufSize,long lCodeType,bool bBgTransparent = false );

		static long		 Save2Image(HBITMAP hBitMap,const char * szFilePath,bool bBgTransparent = false );
		static long		 Save2ImageBuf(HBITMAP hBitMap,char *&szImageBuf,long &lImageBufSize,long lCodeType,bool bBgTransparent = false);
		static long		 FreeImageBuf(char *&szImageBuf);

	public:
		SmtRenderBuf &operator = (const SmtRenderBuf &other);

	protected:
		HWND			 m_hWnd;										//窗口句柄
		HDC				 m_hPaintDC;									//临时 DC
		HBITMAP          m_hOldPaintBuf;								//临时 BMP
		HBITMAP          m_hPaintBuf;									//buf BMP

		long             m_nBufWidth;									//buf宽度
		long             m_nBufHeight;									//buf高度
		bool			 m_bOnwerBuf;									//拥有权

#ifdef SMT_THREAD_SAFE
		SmtCSLock		 m_cslock;										//多线程安全
#endif
	};
} 
#endif //_GDI_DC_BUF_H