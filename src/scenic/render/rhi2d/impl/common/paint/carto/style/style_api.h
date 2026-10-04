// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_STYLE_API_H_
#define SCENIC_RHI2D_STYLE_API_H_

#include "gis/envelope.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_pod.h"
#include "scenic/render/rhi2d/public/device/viewport.h"
#include "base/math/math.h"
#include "scenic/render/scenic_impl_export.h"

// Scenic-local viewport / envelope / LOGFONT helpers. Replaces
// gis/style/smt/style_api.h for scenic_copy TUs.

SCENIC_IMPL_EXPORT void viewport_to_rect(base::lRect& lrect,
                                           const base::Viewport& viewport);
SCENIC_IMPL_EXPORT void windowport_to_rect(
    base::fRect& frect, const base::Windowport& windowport);
SCENIC_IMPL_EXPORT void envelope_to_rect(base::fRect& frect,
                                           const gis::Envelope& env);
SCENIC_IMPL_EXPORT void rect_to_envelope(gis::Envelope& env,
                                           const base::fRect& frect);
SCENIC_IMPL_EXPORT void anno_desc_to_log_font(
    LOGFONT& lf, const base::AnnotationDesc& anno);
SCENIC_IMPL_EXPORT void log_font_to_anno_desc(base::AnnotationDesc& anno,
                                                const LOGFONT& lf);

#endif  // SCENIC_RHI2D_STYLE_API_H_
