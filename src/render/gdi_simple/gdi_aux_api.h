/*
File:    gdi_aux_api.h

Desc:    GDI µØÍ¼äÖÈ¾¸¨Öúº¯Êý¿â

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GDI_AUX_API_H
#define _GDI_AUX_API_H

#include "core.h"
#include "bas_struct.h"

using namespace Smt_Core;

void ClearRect(HDC hDC,int x,int y,int w,int h,COLORREF clr = RGB(255,255,255));
void DrawRect (HDC hDC,RECT &rect,BOOL  use_xor = TRUE);
void DrawRect (HDC hDC,lRect &lrect,BOOL  use_xor = TRUE);

void DrawLine(HDC hDC,lPoint *plPoints,int nCount,BOOL  use_xor = TRUE);
void DrawLine(HDC hDC,POINT  *pPoints,int nCount,BOOL  use_xor = TRUE);

void DrawCross(HDC hDC,long lX,long lY,long r,BOOL  use_xor = TRUE);


#endif //_GDI_AUX_API_H