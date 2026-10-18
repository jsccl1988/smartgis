// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Scalar geometry results: length, area, distance, and emptiness.

#include "content/browser/glang.runtime/language/ir/gis.h"

#include "content/browser/glang.runtime/language/ir/gis/eval/write.h"
#include "gis/analysis/ops/ir/ir_text.h"
#include "gis/analysis/ops/wkt/wkt_codec.h"

namespace content {
namespace ir {

bool distance(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error) {
  int32_t crs = 0;
  if (!gis::detail::require_same_epsg(a_epsg, b_epsg, &crs, error)) {
    return false;
  }
  gis::detail::OgrGeom left = gis::detail::geometry_from_wkt(a, error);
  if (!left) {
    return false;
  }
  if (gis::detail::is_geographic_epsg(crs)) {
    return gis::detail::reject(error, "geographic crs");
  }
  gis::detail::OgrGeom right = gis::detail::geometry_from_wkt(b, error);
  if (!right) {
    return false;
  }
  const gis::detail::IrValue value =
      detail::eval_geometry("native.distance", left.get(), right.get(), 0.0);
  if (value.kind != gis::detail::IrKind::kNumber) {
    return gis::detail::reject(error, "invalid geometry");
  }
  return gis::detail::accept(out, gis::detail::format_number(value.number));
}

bool area(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  int32_t crs = 0;
  if (!gis::detail::require_epsg(crs_epsg, &crs, error)) {
    return false;
  }
  gis::detail::OgrGeom geom = gis::detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  if (gis::detail::is_geographic_epsg(crs)) {
    return gis::detail::reject(error, "geographic crs");
  }
  const gis::detail::IrValue value = detail::eval_geometry("native.area", geom.get(), nullptr, 0.0);
  if (value.kind != gis::detail::IrKind::kNumber) {
    return gis::detail::reject(error, "invalid geometry");
  }
  return gis::detail::accept(out, gis::detail::format_number(value.number));
}

bool length(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  int32_t crs = 0;
  if (!gis::detail::require_epsg(crs_epsg, &crs, error)) {
    return false;
  }
  gis::detail::OgrGeom geom = gis::detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  if (gis::detail::is_geographic_epsg(crs)) {
    return gis::detail::reject(error, "geographic crs");
  }
  const gis::detail::IrValue value =
      detail::eval_geometry("native.length", geom.get(), nullptr, 0.0);
  if (value.kind != gis::detail::IrKind::kNumber) {
    return gis::detail::reject(error, "invalid geometry");
  }
  return gis::detail::accept(out, gis::detail::format_number(value.number));
}

bool is_empty(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  if (!gis::detail::require_epsg(crs_epsg, nullptr, error)) {
    return false;
  }
  gis::detail::OgrGeom geom = gis::detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  const gis::detail::IrValue value =
      detail::eval_geometry("native.is_empty", geom.get(), nullptr, 0.0);
  if (value.kind != gis::detail::IrKind::kFlag) {
    return gis::detail::reject(error, "invalid geometry");
  }
  return gis::detail::accept(out, value.flag ? "1" : "0");
}

}  // namespace ir
}  // namespace content
