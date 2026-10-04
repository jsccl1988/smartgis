// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_PRESENT_CARTO_SMT_STYLE_FROM_PAINT_H_
#define SMT_LEGACY_GIS_PRESENT_CARTO_SMT_STYLE_FROM_PAINT_H_

#include "gis/carto/style/paint_resolve.h"
#include "gis/gis_export.h"
#include "legacy/gis/present/carto/style.h"

namespace gis {
namespace style {

GIS_EXPORT base::SmtStyle to_smt_style(const ResolvedPaint& paint,
                                       const char* name);

}  // namespace style
}  // namespace gis

#endif
