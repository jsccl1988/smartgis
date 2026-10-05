// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_STYLE_OGR_H_
#define SCENIC_RHI2D_STYLE_OGR_H_

#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_pod.h"
#include "scenic/render/scenic_impl_export.h"

class OGRFeature;

namespace scenic {
namespace detail {

// OGR feature ↔ scenic Style for the scenic_copy map path.
SCENIC_IMPL_EXPORT void copy_style_to_ogr(const Style* src, OGRFeature* dst);
SCENIC_IMPL_EXPORT Style* copy_style_from_ogr(OGRFeature* src);
SCENIC_IMPL_EXPORT void fill_default_draw_style(OGRFeature* src, Style* dst,
                                                  float fblc);

}  // namespace detail
}  // namespace scenic

namespace gis {
namespace datasource {

// Forwards used by map_painter / map_to_scene. Distinct from
// copy_smt_style_to_ogr(base::Style*) in ogr_feature_codec (leftover ABI).
inline void copy_style_to_ogr(const base::Style* src, OGRFeature* dst) {
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

#endif  // SCENIC_RHI2D_STYLE_OGR_H_
