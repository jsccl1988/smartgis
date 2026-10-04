// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_api.h"

#include <cstring>

void viewport_to_rect(base::lRect& lrect, const base::Viewport& viewport) {
  lrect.lb.x = static_cast<long>(viewport.m_fVOX);
  lrect.lb.y = static_cast<long>(viewport.m_fVOY);
  lrect.rt.x = static_cast<long>(viewport.m_fVOX + viewport.m_fVWidth);
  lrect.rt.y = static_cast<long>(viewport.m_fVOY + viewport.m_fVHeight);
}

void windowport_to_rect(base::fRect& frect,
                        const base::Windowport& windowport) {
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

void anno_desc_to_log_font(LOGFONT& lf, const base::AnnotationDesc& anno) {
  lf.lfHeight = static_cast<LONG>(anno.fHeight);
  lf.lfWidth = static_cast<LONG>(anno.fWidth);
  lf.lfEscapement = anno.lEscapement;
  lf.lfOrientation = anno.lOrientation;
  lf.lfWeight = anno.lWeight;
  lf.lfItalic = static_cast<BYTE>(anno.lItalic);
  lf.lfUnderline = static_cast<BYTE>(anno.lUnderline);
  lf.lfStrikeOut = static_cast<BYTE>(anno.lStrikeOut);
  lf.lfCharSet = static_cast<BYTE>(anno.lCharSet);
  lf.lfOutPrecision = static_cast<BYTE>(anno.lOutPrecision);
  lf.lfClipPrecision = static_cast<BYTE>(anno.lClipPrecision);
  lf.lfQuality = static_cast<BYTE>(anno.lQuality);
  lf.lfPitchAndFamily = static_cast<BYTE>(anno.lPitchAndFamily);
  std::strcpy(lf.lfFaceName, anno.szFaceName);
}

void log_font_to_anno_desc(base::AnnotationDesc& anno, const LOGFONT& lf) {
  anno.fHeight = static_cast<float>(lf.lfHeight);
  anno.fWidth = static_cast<float>(lf.lfWidth);
  anno.lEscapement = lf.lfEscapement;
  anno.lOrientation = lf.lfOrientation;
  anno.lWeight = lf.lfWeight;
  anno.lItalic = lf.lfItalic;
  anno.lUnderline = lf.lfUnderline;
  anno.lStrikeOut = lf.lfStrikeOut;
  anno.lCharSet = lf.lfCharSet;
  anno.lOutPrecision = lf.lfOutPrecision;
  anno.lClipPrecision = lf.lfClipPrecision;
  anno.lQuality = lf.lfQuality;
  anno.lPitchAndFamily = lf.lfPitchAndFamily;
  std::strcpy(anno.szFaceName, lf.lfFaceName);
}
