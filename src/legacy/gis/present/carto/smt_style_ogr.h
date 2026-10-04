// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_PRESENT_CARTO_SMT_STYLE_OGR_H_
#define SMT_LEGACY_GIS_PRESENT_CARTO_SMT_STYLE_OGR_H_

#include "gis/gis_export.h"

class OGRFeature;

namespace base {
class SmtStyle;
}

namespace gis {
namespace datasource {

GIS_EXPORT void copy_smt_style_to_ogr(const base::SmtStyle* src,
                                      OGRFeature* dst);
GIS_EXPORT base::SmtStyle* copy_ogr_style_from_ogr(OGRFeature* src);

// Pen/brush (+ optional anno) when GeoJSON has no binary "style" blob.
GIS_EXPORT void fill_default_draw_style(OGRFeature* src, base::SmtStyle* dst,
                                        float fblc);

}  // namespace datasource
}  // namespace gis

#endif  // SMT_LEGACY_GIS_PRESENT_CARTO_SMT_STYLE_OGR_H_
