// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_PRESENT_CARTO_STYLE_API_H_
#define GIS_PRESENT_CARTO_STYLE_API_H_

#include "gis/gis_export.h"
#include "gis/model/envelope.h"
#include "legacy/gis/present/carto/style.h"
#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/core/types/types.h"
#include "legacy/core/macros/macros.h"

//////////////////////////////////////////////////////////////////////////
void GIS_EXPORT viewport_to_rect(base::lRect& lrect,
                                 const base::Viewport& viewport);
void GIS_EXPORT windowport_to_rect(base::fRect& frect,
                                   const base::Windowport& windowport);

void GIS_EXPORT envelope_to_rect(base::fRect& frect, const gis::Envelope& env);
void GIS_EXPORT rect_to_envelope(gis::Envelope& env, const base::fRect& frect);

void GIS_EXPORT anno_desc_to_log_font(LOGFONT& lf,
                                      const base::SmtAnnotationDesc& annoDesc);
void GIS_EXPORT log_font_to_anno_desc(base::SmtAnnotationDesc& annoDesc,
                                      const LOGFONT& lf);

#endif  // GIS_PRESENT_CARTO_STYLE_API_H_
