// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_STYLE_OGR_H_
#define SCENIC_DETAIL_STYLE_OGR_H_

#include "scenic/detail/style.h"
#include "scenic/scenic_impl_export.h"

class OGRFeature;

namespace scenic {
namespace detail {

// OGR feature ↔ scenic Style. Replaces leftover smt_style_ogr for scenic_copy.
LEGACY_RENDER_EXPORT void copy_style_to_ogr(const Style* src, OGRFeature* dst);
LEGACY_RENDER_EXPORT Style* copy_style_from_ogr(OGRFeature* src);
LEGACY_RENDER_EXPORT void fill_default_draw_style(OGRFeature* src, Style* dst,
                                                  float fblc);

}  // namespace detail
}  // namespace scenic

namespace gis {
namespace datasource {

// Historical entry points used by map_painter / map_to_scene.
inline void copy_smt_style_to_ogr(const base::Style* src, OGRFeature* dst) {
  ::scenic::detail::copy_style_to_ogr(src, dst);
}
inline base::Style* copy_ogr_style_from_ogr(OGRFeature* src) {
  return ::scenic::detail::copy_style_from_ogr(src);
}
inline void fill_default_draw_style(OGRFeature* src, base::Style* dst,
                                    float fblc) {
  ::scenic::detail::fill_default_draw_style(src, dst, fblc);
}

}  // namespace datasource
}  // namespace gis

#endif  // SCENIC_DETAIL_STYLE_OGR_H_
