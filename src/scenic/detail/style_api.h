// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_STYLE_API_H_
#define SCENIC_DETAIL_STYLE_API_H_

#include "gis/envelope.h"
#include "scenic/detail/style.h"
#include "scenic/detail/viewport.h"
#include "scenic/detail/geom.h"
#include "scenic/scenic_impl_export.h"

// Scenic-local viewport / envelope / LOGFONT helpers. Replaces
// legacy/gis/present/carto/style_api.h for scenic_copy TUs.

LEGACY_RENDER_EXPORT void viewport_to_rect(base::lRect& lrect,
                                           const base::Viewport& viewport);
LEGACY_RENDER_EXPORT void windowport_to_rect(
    base::fRect& frect, const base::Windowport& windowport);
LEGACY_RENDER_EXPORT void envelope_to_rect(base::fRect& frect,
                                           const gis::Envelope& env);
LEGACY_RENDER_EXPORT void rect_to_envelope(gis::Envelope& env,
                                           const base::fRect& frect);
LEGACY_RENDER_EXPORT void anno_desc_to_log_font(
    LOGFONT& lf, const base::AnnotationDesc& anno);
LEGACY_RENDER_EXPORT void log_font_to_anno_desc(base::AnnotationDesc& anno,
                                                const LOGFONT& lf);

#endif  // SCENIC_DETAIL_STYLE_API_H_
