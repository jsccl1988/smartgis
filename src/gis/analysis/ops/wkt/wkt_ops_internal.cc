// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/wkt/wkt_ops_internal.h"

#include "gis/analysis/ops/ir/ir_text.h"
#include "gis/analysis/ops/wkt/wkt_codec.h"
#include "gis/geo/ops/geometry_traits.h"

#include <optional>
#include <string>
#include <utility>

namespace gis {
namespace detail {

bool write_geometry(const OGRGeometry& src,
                    IrValue value,
                    std::string* out,
                    std::string* error) {
  if (value.kind != IrKind::kGeometry || !value.geometry) {
    return reject(error, "invalid geometry");
  }
  value.geometry->setCoordinateDimension(
      geo::geometry_traits<OGRGeometry>::coordinate_dimension(src));
  const std::string text = geometry_to_wkt(value.geometry.get());
  if (text.empty()) {
    return reject(error, "invalid geometry");
  }
  return accept(out, text);
}

IrValue eval_env(const char* id, const IrEnv& env) {
  std::optional<IrValue> hit = eval_geometry_ir(id, env);
  if (!hit) {
    return {};
  }
  return std::move(*hit);
}

IrValue eval_geometry(const char* id, OGRGeometry* input, OGRGeometry* other, double number) {
  IrEnv env;
  env.input = input;
  env.other = other;
  env.number = number;
  return eval_env(id, env);
}

bool construct_pair(const char* id,
                    std::string_view a,
                    int32_t a_epsg,
                    std::string_view b,
                    int32_t b_epsg,
                    std::string* out,
                    std::string* error) {
  if (!require_same_epsg(a_epsg, b_epsg, nullptr, error)) {
    return false;
  }
  OgrGeom left = geometry_from_wkt(a, error);
  if (!left) {
    return false;
  }
  OgrGeom right = geometry_from_wkt(b, error);
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
  if (!require_epsg(crs_epsg, nullptr, error)) {
    return false;
  }
  OgrGeom geom = geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  if (reject_empty && geom->IsEmpty()) {
    return reject(error, "empty geometry");
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
  if (!require_same_epsg(wkt_epsg, distance_epsg, &crs, error)) {
    return false;
  }
  if (is_geographic_epsg(crs)) {
    return reject(error, "geographic crs");
  }
  OgrGeom geom = geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  double width = 0;
  if (!parse_finite(distance, &width) || !(width > 0.0)) {
    return reject(error, "distance");
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
  if (!require_same_epsg(a_epsg, b_epsg, nullptr, error)) {
    return false;
  }
  OgrGeom left = geometry_from_wkt(a, error);
  if (!left) {
    return false;
  }
  OgrGeom right = geometry_from_wkt(b, error);
  if (!right) {
    return false;
  }
  const IrValue value = eval_geometry(id, left.get(), right.get(), 0.0);
  if (value.kind != IrKind::kFlag) {
    return reject(error, "invalid geometry");
  }
  return accept(out, value.flag ? "1" : "0");
}

}  // namespace detail
}  // namespace gis
