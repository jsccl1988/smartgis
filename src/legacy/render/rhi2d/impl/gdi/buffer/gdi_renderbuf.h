// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GDI_DC_BUF_H
#define _GDI_DC_BUF_H

#include <mutex>

#include "legacy/core/core.h"
using namespace base;
namespace render {
enum eSwapType {
  BLT_STRETCH,
  BLT_TRANSPARENT,
};

class SmtRenderBuf {
 public:
  SmtRenderBuf(void);
  SmtRenderBuf(HWND hWnd);
  ~SmtRenderBuf(void);

 public:
  inline HWND GetWnd() const { return m_hWnd; }
  inline long SetWnd(HWND hWnd);

  long SetBufSize(int cx, int cy);
  inline long GetBufWidth(void) const { return m_nBufWidth; }
  inline long GetBufHeight(void) const { return m_nBufHeight; }

  inline HBITMAP GetBuf(void) { return m_hPaintBuf; }
  long ShareBuf(
      SmtRenderBuf &
          rbSrc);

  long SwapBuf(int destOrgx, int destOrgy, int destW, int destH, int srcOrgx,
               int srcOrgy, int op = SRCCOPY);
  long SwapBuf(int destOrgx, int destOrgy, int destW, int destH, int srcOrgx,
               int srcOrgy, int srcW, int srcH,
               eSwapType type = BLT_TRANSPARENT, int op = SRCCOPY,
               // Key must NOT match ClearBuf ocean (170,211,223) or the whole
               // map buffer is treated as transparent and the HWND stays white.
               COLORREF clr = RGB(255, 255, 255));  // Transparent/Stretch

  long SwapBuf(SmtRenderBuf &rbTarget, int destOrgx, int destOrgy, int destW,
               int destH, int srcOrgx, int srcOrgy, int op = SRCCOPY);
  long SwapBuf(SmtRenderBuf &rbTarget, int destOrgx, int destOrgy, int destW,
               int destH, int srcOrgx, int srcOrgy, int srcW, int srcH,
               eSwapType type = BLT_TRANSPARENT, int op = SRCCOPY,
               COLORREF clr = RGB(255, 255, 255));  // Transparent/Stretch

  HDC PrepareDC(
      bool bClip =
          true);
  long EndDC(void);

  long ClearBuf(int x, int y, int w, int h, COLORREF clr = RGB(170, 211, 223));

  long DrawImage(const char *szImageBuf, int nImageBufSize, long lCodeType,
                 long x = 0, long y = 0, long cx = -1, long cy = -1);
  long StrethImage(const char *szImageBuf, int nImageBufSize, long lCodeType,
                   long xoffset, long yoffset, long xsize, long ysize,
                   DWORD dwRop = SRCCOPY);

  long Save2Image(const char *szFilePath, bool bBgTransparent = false);
  long Save2ImageBuf(char *&szImageBuf, long &lImageBufSize, long lCodeType,
                     bool bBgTransparent = false);

  static long Save2Image(HBITMAP hBitMap, const char *szFilePath,
                         bool bBgTransparent = false);
  static long Save2ImageBuf(HBITMAP hBitMap, char *&szImageBuf,
                            long &lImageBufSize, long lCodeType,
                            bool bBgTransparent = false);
  static long FreeImageBuf(char *&szImageBuf);

 public:
  SmtRenderBuf &operator=(const SmtRenderBuf &other);

 protected:
  HWND m_hWnd;
  HDC m_hPaintDC;
  HBITMAP m_hOldPaintBuf;
  HBITMAP m_hPaintBuf;     // buf BMP

  long m_nBufWidth;
  long m_nBufHeight;
  bool m_bOnwerBuf;

#ifdef SMT_THREAD_SAFE
  std::mutex m_cslock;
#endif
};
}  // namespace render
#endif  //_GDI_DC_BUF_H