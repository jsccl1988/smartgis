/*
File:    gdi_aux_api.h

Desc:    GDI 2D render helper routines.

Version: Version 1.0

Writter:  legacy

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GDI_AUX_API_H
#define _GDI_AUX_API_H

#include "base/core/bas_struct.h"
#include "base/core/core.h"

using namespace base;

void clear_rect(HDC hDC, int x, int y, int w, int h,
                COLORREF clr = RGB(255, 255, 255));
void draw_rect(HDC hDC, RECT& rect, BOOL exclusive = TRUE);
void draw_rect(HDC hDC, lRect& lrect, BOOL exclusive = TRUE);

void draw_line(HDC hDC, lPoint* plPoints, int nCount, BOOL exclusive = TRUE);
void draw_line(HDC hDC, POINT* pPoints, int nCount, BOOL exclusive = TRUE);

void draw_cross(HDC hDC, long lX, long lY, long r, BOOL exclusive = TRUE);

// UTF-8 (GeoJSON/OGR) or ACP leftover text → TextOutW.
void draw_anno_text(HDC hdc, long x, long y, const char* text, int px_h = 14,
                    int halo_px = 2);
void draw_point_disc(HDC hdc, long x, long y, int radius);

#endif  // _GDI_AUX_API_H
