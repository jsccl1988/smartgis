#include "gdi_aux_api.h"

void ClearRect(HDC hDC,int x,int y,int w,int h,COLORREF clr)
{
	RECT rect;
	rect.left   = x;
	rect.bottom = y;
	rect.right  = x + w;
	rect.top    = y + h;

	HBRUSH hBrush = CreateSolidBrush( clr);
	HBRUSH hOldBrush = (HBRUSH)::SelectObject(hDC,hBrush);
	::FillRect(hDC,&rect,hBrush);
	::SelectObject(hDC,hOldBrush);
	::DeleteObject(hBrush);
}

void DrawRect (HDC hDC,RECT &rect,BOOL  xor)
{
	MoveToEx (hDC, rect.left,  rect.top, NULL) ;
	LineTo(hDC,rect.right, rect.top);
	LineTo(hDC,rect.right, rect.bottom);
	LineTo(hDC,rect.left,  rect.bottom);
	LineTo(hDC,rect.left,  rect.top);
}
void DrawRect (HDC hDC,lRect &lrect,BOOL  xor)
{
	MoveToEx (hDC, lrect.lb.x,  lrect.lb.y, NULL) ;
	LineTo(hDC,lrect.rt.x, lrect.lb.y);
	LineTo(hDC,lrect.rt.x, lrect.rt.y);
	LineTo(hDC,lrect.lb.x,lrect.rt.y);
	LineTo(hDC,lrect.lb.x,  lrect.lb.y);
}

void DrawLine(HDC hDC,Smt_Core::lPoint *plPoints,int nCount,BOOL  xor )
{
	if (plPoints == NULL)
		return;

	MoveToEx (hDC, plPoints[0].x, plPoints[0].y, NULL) ;    
	PolylineTo(hDC, (POINT*)plPoints, nCount);
}

void DrawLine(HDC hDC,POINT  *pPoints,int nCount,BOOL  xor)
{
	if (pPoints == NULL)
		return;

	MoveToEx (hDC, pPoints[0].x, pPoints[0].y, NULL) ;    
	PolylineTo(hDC, pPoints, nCount);
}

void DrawCross(HDC hDC,long lX,long lY,long r,BOOL  xor)
{
	MoveToEx (hDC, lX-r, lY, NULL) ;
	LineTo(hDC,lX+r, lY);

	MoveToEx (hDC, lX, lY, NULL) ;
	LineTo(hDC,lX, lY);
}
