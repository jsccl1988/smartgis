/*
File:    bl_api.h

Desc:    SmtBaseLib leftover style / viewport helpers

Version: Version 1.0

Writter:  CCL

Date:    2010.11.17

Copyright (c) 2026 The Mogu Authors.
All rights reserved.
*/
#ifndef _BL_API_H
#define _BL_API_H

#include "base/core/export.h"
#include "gis/model/envelope.h"
#include "legacy/carto/style.h"
#include "legacy/carto/style_bas_struct.h"
#include "legacy/core/bas_struct.h"
#include "legacy/core/core.h"

//////////////////////////////////////////////////////////////////////////
void BASE_EXPORT viewport_to_rect(base::lRect& lrect,
                                  const base::Viewport& viewport);
void BASE_EXPORT windowport_to_rect(base::fRect& frect,
                                    const base::Windowport& windowport);

void BASE_EXPORT envelope_to_rect(base::fRect& frect, const gis::Envelope& env);
void BASE_EXPORT rect_to_envelope(gis::Envelope& env, const base::fRect& frect);

void BASE_EXPORT
anno_desc_to_log_font(LOGFONT& lf, const base::SmtAnnotationDesc& annoDesc);
void BASE_EXPORT log_font_to_anno_desc(base::SmtAnnotationDesc& annoDesc,
                                       const LOGFONT& lf);

#if !defined(BASE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "base_d.lib")
#else
#pragma comment(lib, "base.lib")
#endif
#endif

#endif  //_SMT_API_H
