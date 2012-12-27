#include "bl_api.h"

using namespace Smt_Core;
using namespace Smt_Base;

void ViewportToRect(lRect &lrect,const Viewport &viewport)
{
   lrect.lb.x = viewport.m_fVOX;
   lrect.lb.y = viewport.m_fVOY;

   lrect.rt.x = viewport.m_fVOX + viewport.m_fVWidth;
   lrect.rt.y = viewport.m_fVOY + viewport.m_fVHeight;
}

void WindowportToRect(fRect &frect,const Windowport &windowport)
{
	frect.lb.x = windowport.m_fWOX;
	frect.lb.y = windowport.m_fWOY;

	frect.rt.x = windowport.m_fWOX + windowport.m_fWWidth;
	frect.rt.y = windowport.m_fWOY + windowport.m_fWHeight;
}

void   EnvelopeToRect(fRect &frect,const Envelope &env)
{
    frect.lb.x = env.MinX;
	frect.lb.y = env.MinY;

	frect.rt.x   = env.MaxX;
	frect.rt.y   = env.MaxY;
}

void   RectToEnvelope(Envelope &env,const fRect &frect)
{
	env.Merge(frect.lb.x,frect.lb.y);
	env.Merge(frect.rt.x,frect.rt.y);
}

void	AnnoDescToLogFont(LOGFONT &lf,const Smt_Base::SmtAnnotationDesc &annoDesc)
{
	lf.lfHeight = annoDesc.fHeight;
	lf.lfWidth = annoDesc.fWidth;
	lf.lfEscapement = annoDesc.lEscapement;
	lf.lfOrientation = annoDesc.lOrientation;
	lf.lfWeight = annoDesc.lWeight;
	lf.lfItalic = annoDesc.lItalic;
	lf.lfUnderline = annoDesc.lUnderline;
	lf.lfStrikeOut = annoDesc.lStrikeOut;
	lf.lfCharSet = annoDesc.lCharSet;
	lf.lfOutPrecision = annoDesc.lOutPrecision;
	lf.lfClipPrecision = annoDesc.lClipPrecision;
	lf.lfQuality = annoDesc.lQuality;
	lf.lfPitchAndFamily = annoDesc.lPitchAndFamily;
	strcpy (lf.lfFaceName,annoDesc.szFaceName);
}

void	LogFontToAnnoDesc(Smt_Base::SmtAnnotationDesc &annoDesc,const LOGFONT &lf)
{
	annoDesc.fHeight = lf.lfHeight;
	annoDesc.fWidth  = lf.lfWidth;
	annoDesc.lEscapement = lf.lfEscapement;
	annoDesc.lOrientation = lf.lfOrientation;
	annoDesc.lWeight = lf.lfWeight;
	annoDesc.lItalic = lf.lfItalic;
	annoDesc.lUnderline = lf.lfUnderline;
	annoDesc.lStrikeOut = lf.lfStrikeOut;
	annoDesc.lCharSet = lf.lfCharSet;
	annoDesc.lOutPrecision = lf.lfOutPrecision;
	annoDesc.lClipPrecision = lf.lfClipPrecision;
	annoDesc.lQuality = lf.lfQuality;
	annoDesc.lPitchAndFamily=lf.lfPitchAndFamily;
	strcpy (annoDesc.szFaceName, lf.lfFaceName);
}