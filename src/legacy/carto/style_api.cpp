// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/carto/style_api.h"

void viewport_to_rect(base::lRect& lrect, const base::Viewport& viewport) {
  lrect.lb.x = viewport.m_fVOX;
  lrect.lb.y = viewport.m_fVOY;

  lrect.rt.x = viewport.m_fVOX + viewport.m_fVWidth;
  lrect.rt.y = viewport.m_fVOY + viewport.m_fVHeight;
}

void windowport_to_rect(base::fRect& frect, const base::Windowport& windowport) {
  frect.lb.x = windowport.m_fWOX;
  frect.lb.y = windowport.m_fWOY;

  frect.rt.x = windowport.m_fWOX + windowport.m_fWWidth;
  frect.rt.y = windowport.m_fWOY + windowport.m_fWHeight;
}

void envelope_to_rect(base::fRect& frect, const gis::Envelope& env) {
  frect.lb.x = static_cast<float>(env.MinX);
  frect.lb.y = static_cast<float>(env.MinY);

  frect.rt.x = static_cast<float>(env.MaxX);
  frect.rt.y = static_cast<float>(env.MaxY);
}

void rect_to_envelope(gis::Envelope& env, const base::fRect& frect) {
  env.merge(frect.lb.x, frect.lb.y);
  env.merge(frect.rt.x, frect.rt.y);
}

void anno_desc_to_log_font(LOGFONT& lf,
                           const base::SmtAnnotationDesc& annoDesc) {
  lf.lfHeight = static_cast<LONG>(annoDesc.fHeight);
  lf.lfWidth = static_cast<LONG>(annoDesc.fWidth);
  lf.lfEscapement = annoDesc.lEscapement;
  lf.lfOrientation = annoDesc.lOrientation;
  lf.lfWeight = annoDesc.lWeight;
  lf.lfItalic = static_cast<BYTE>(annoDesc.lItalic);
  lf.lfUnderline = static_cast<BYTE>(annoDesc.lUnderline);
  lf.lfStrikeOut = static_cast<BYTE>(annoDesc.lStrikeOut);
  lf.lfCharSet = static_cast<BYTE>(annoDesc.lCharSet);
  lf.lfOutPrecision = static_cast<BYTE>(annoDesc.lOutPrecision);
  lf.lfClipPrecision = static_cast<BYTE>(annoDesc.lClipPrecision);
  lf.lfQuality = static_cast<BYTE>(annoDesc.lQuality);
  lf.lfPitchAndFamily = static_cast<BYTE>(annoDesc.lPitchAndFamily);
  strcpy(lf.lfFaceName, annoDesc.szFaceName);
}

void log_font_to_anno_desc(base::SmtAnnotationDesc& annoDesc,
                           const LOGFONT& lf) {
  annoDesc.fHeight = static_cast<float>(lf.lfHeight);
  annoDesc.fWidth = static_cast<float>(lf.lfWidth);
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
  annoDesc.lPitchAndFamily = lf.lfPitchAndFamily;
  strcpy(annoDesc.szFaceName, lf.lfFaceName);
}
