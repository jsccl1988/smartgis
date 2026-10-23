// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/gis/geom/construct.h"

#include "content/browser/glang.runtime/exec/op/gis/geom/write.h"
#include "gis/analysis/ops/ir/ir_text.h"
#include "gis/analysis/ops/wkt/wkt_codec.h"

namespace content {
namespace code {
namespace detail {

bool construct_pair(const char* id,
                    std::string_view a,
                    int32_t a_epsg,
                    std::string_view b,
                    int32_t b_epsg,
                    std::string* out,
                    std::string* error) {
  if (!gis::detail::require_same_epsg(a_epsg, b_epsg, nullptr, error)) {
    return false;
  }
  gis::detail::OgrGeom left = gis::detail::geometry_from_wkt(a, error);
  if (!left) {
    return false;
  }
  gis::detail::OgrGeom right = gis::detail::geometry_from_wkt(b, error);
  if (!right) {
    return false;
  }
  return write_geometry(*left, eval_geometry(id, left.get(), right.get(), 0.0), out, error);
}

bool construct_shape(const char* id,
                     std::string_view wkt,
                     int32_t crs_epsg,
                     bool reject_empty,
                     std::string* out,
                     std::string* error) {
  if (!gis::detail::require_epsg(crs_epsg, nullptr, error)) {
    return false;
  }
  gis::detail::OgrGeom geom = gis::detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  if (reject_empty && geom->IsEmpty()) {
    return gis::detail::reject(error, "empty geometry");
  }
  return write_geometry(*geom, eval_geometry(id, geom.get(), nullptr, 0.0), out, error);
}

bool construct_width(const char* id,
                     std::string_view wkt,
                     int32_t wkt_epsg,
                     std::string_view distance,
                     int32_t distance_epsg,
                     std::string* out,
                     std::string* error) {
  int32_t crs = 0;
  if (!gis::detail::require_same_epsg(wkt_epsg, distance_epsg, &crs, error)) {
    return false;
  }
  if (gis::detail::is_geographic_epsg(crs)) {
    return gis::detail::reject(error, "geographic crs");
  }
  gis::detail::OgrGeom geom = gis::detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  double width = 0;
  if (!gis::detail::parse_finite(distance, &width) || !(width > 0.0)) {
    return gis::detail::reject(error, "distance");
  }
  return write_geometry(*geom, eval_geometry(id, geom.get(), nullptr, width), out, error);
}

bool predicate_text(const char* id,
                    std::string_view a,
                    int32_t a_epsg,
                    std::string_view b,
                    int32_t b_epsg,
                    std::string* out,
                    std::string* error) {
  if (!gis::detail::require_same_epsg(a_epsg, b_epsg, nullptr, error)) {
    return false;
  }
  gis::detail::OgrGeom left = gis::detail::geometry_from_wkt(a, error);
  if (!left) {
    return false;
  }
  gis::detail::OgrGeom right = gis::detail::geometry_from_wkt(b, error);
  if (!right) {
    return false;
  }
  const gis::detail::IrValue value = eval_geometry(id, left.get(), right.get(), 0.0);
  if (value.kind != gis::detail::IrKind::kFlag) {
    return gis::detail::reject(error, "invalid geometry");
  }
  return gis::detail::accept(out, value.flag ? "1" : "0");
}

}  // namespace detail
}  // namespace code
}  // namespace content
