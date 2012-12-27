#include "rd_renderdevice.h"

using namespace Smt_Core;

namespace Smt_Rd
{
	//void SmtRenderDevice::LPToDP(float x,float y,LONG &X,LONG &Y) 
	//{	
	//	if (m_Windowport.m_fWWidth == 0 || m_Windowport.m_fWHeight == 0 || m_Viewport.m_fVWidth == 0 || m_Viewport.m_fVHeight == 0)
	//	{
	//		X = x;
	//		Y = y;
	//		return;
	//	}

	//	float xblc,yblc,blc;
	//	xblc = m_Viewport.m_fVWidth/m_Windowport.m_fWWidth;
	//	yblc = m_Viewport.m_fVHeight/m_Windowport.m_fWHeight;

	//	blc = (xblc > yblc)?yblc:xblc;

	//	X=m_Viewport.m_fVOX+(x-m_Windowport.m_fOX)*blc;
	//	Y=m_Viewport.m_fVOY+(y-m_Windowport.m_fOY)*blc;

	//	//if (m_nMapMode == MM_TEXT)
	//	Y = m_Viewport.m_fVHeight - Y;
	//}

	//void SmtRenderDevice::DPToLP(LONG X,LONG Y,float &x,float &y)
	//{
	//	if (m_Windowport.m_fWWidth == 0 || m_Windowport.m_fWHeight == 0 || m_Viewport.m_fVWidth == 0 || m_Viewport.m_fVHeight == 0)
	//	{
	//		X = x;
	//		Y = y;
	//		return;
	//	}

	//	//	if (m_nMapMode == MM_TEXT)
	//	Y = m_Viewport.m_fVHeight - Y;

	//	float xblc,yblc,blc;
	//	xblc = m_Viewport.m_fVWidth/m_Windowport.m_fWWidth;
	//	yblc = m_Viewport.m_fVHeight/m_Windowport.m_fWHeight;

	//	blc = (xblc > yblc)?yblc:xblc;

	//	x = (X-m_Viewport.m_fVOX)/blc + m_Windowport.m_fOX;
	//	y = (Y-m_Viewport.m_fVOY)/blc + m_Windowport.m_fOY;
	//}

	//void SmtRenderDevice::LRectTodbfRect(fRect frect,lRect &lrect)
	//{
	//	LPToDP(frect.lb.x,frect.lb.y,lrect.lb.x,lrect.lb.y);
	//	LPToDP(frect.rt.x,frect.rt.y,lrect.rt.x,lrect.rt.y);
	//}

	//void SmtRenderDevice::dbDRectToLRect(lRect lrect,fRect &frect)
	//{
	//	DPToLP(lrect.lb.x,lrect.lb.y,frect.lb.x,frect.lb.y);
	//	DPToLP(lrect.rt.x,lrect.rt.y,frect.rt.x,frect.rt.y);
	//}

}