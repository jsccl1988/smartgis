// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/wkt/wkt_ops.h"

#include "gis/analysis/ops/ir/ir_text.h"
#include "gis/analysis/ops/wkt/wkt_codec.h"
#include "gis/analysis/ops/wkt/wkt_ops_internal.h"

namespace gis {

bool buffer(std::string_view wkt,
            int32_t wkt_epsg,
            std::string_view distance,
            int32_t distance_epsg,
            std::string* out,
            std::string* error) {
  return detail::construct_width("native.buffer", wkt, wkt_epsg, distance, distance_epsg, out,
                                 error);
}

bool intersection(std::string_view a,
                  int32_t a_epsg,
                  std::string_view b,
                  int32_t b_epsg,
                  std::string* out,
                  std::string* error) {
  return detail::construct_pair("native.intersection", a, a_epsg, b, b_epsg, out, error);
}

bool geom_union(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error) {
  return detail::construct_pair("native.union", a, a_epsg, b, b_epsg, out, error);
}

bool difference(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error) {
  return detail::construct_pair("native.difference", a, a_epsg, b, b_epsg, out, error);
}

bool sym_difference(std::string_view a,
                    int32_t a_epsg,
                    std::string_view b,
                    int32_t b_epsg,
                    std::string* out,
                    std::string* error) {
  return detail::construct_pair("native.symmetric_difference", a, a_epsg, b, b_epsg, out, error);
}

bool centroid(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  return detail::construct_shape("native.centroid", wkt, crs_epsg, false, out, error);
}

bool envelope(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  return detail::construct_shape("native.envelope", wkt, crs_epsg, true, out, error);
}

bool convex_hull(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  return detail::construct_shape("native.convex_hull", wkt, crs_epsg, false, out, error);
}

bool boundary(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  return detail::construct_shape("native.boundary", wkt, crs_epsg, false, out, error);
}

bool simplify(std::string_view wkt,
              int32_t wkt_epsg,
              std::string_view tolerance,
              int32_t tolerance_epsg,
              std::string* out,
              std::string* error) {
  return detail::construct_width("native.simplify", wkt, wkt_epsg, tolerance, tolerance_epsg,
                                 out, error);
}

bool intersects(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error) {
  return detail::predicate_text("native.intersects", a, a_epsg, b, b_epsg, out, error);
}

bool contains(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error) {
  return detail::predicate_text("native.contains", a, a_epsg, b, b_epsg, out, error);
}

bool within(std::string_view a,
            int32_t a_epsg,
            std::string_view b,
            int32_t b_epsg,
            std::string* out,
            std::string* error) {
  return detail::predicate_text("native.within", a, a_epsg, b, b_epsg, out, error);
}

bool disjoint(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error) {
  return detail::predicate_text("native.disjoint", a, a_epsg, b, b_epsg, out, error);
}

bool equals(std::string_view a,
            int32_t a_epsg,
            std::string_view b,
            int32_t b_epsg,
            std::string* out,
            std::string* error) {
  return detail::predicate_text("native.equals", a, a_epsg, b, b_epsg, out, error);
}

bool touches(std::string_view a,
             int32_t a_epsg,
             std::string_view b,
             int32_t b_epsg,
             std::string* out,
             std::string* error) {
  return detail::predicate_text("native.touches", a, a_epsg, b, b_epsg, out, error);
}

bool crosses(std::string_view a,
             int32_t a_epsg,
             std::string_view b,
             int32_t b_epsg,
             std::string* out,
             std::string* error) {
  return detail::predicate_text("native.crosses", a, a_epsg, b, b_epsg, out, error);
}

bool overlaps(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error) {
  return detail::predicate_text("native.overlaps", a, a_epsg, b, b_epsg, out, error);
}

bool distance(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error) {
  int32_t crs = 0;
  if (!detail::require_same_epsg(a_epsg, b_epsg, &crs, error)) {
    return false;
  }
  detail::OgrGeom left = detail::geometry_from_wkt(a, error);
  if (!left) {
    return false;
  }
  if (detail::is_geographic_epsg(crs)) {
    return detail::reject(error, "geographic crs");
  }
  detail::OgrGeom right = detail::geometry_from_wkt(b, error);
  if (!right) {
    return false;
  }
  const detail::IrValue value =
      detail::eval_geometry("native.distance", left.get(), right.get(), 0.0);
  if (value.kind != detail::IrKind::kNumber) {
    return detail::reject(error, "invalid geometry");
  }
  return detail::accept(out, detail::format_number(value.number));
}

bool area(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  int32_t crs = 0;
  if (!detail::require_epsg(crs_epsg, &crs, error)) {
    return false;
  }
  detail::OgrGeom geom = detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  if (detail::is_geographic_epsg(crs)) {
    return detail::reject(error, "geographic crs");
  }
  const detail::IrValue value = detail::eval_geometry("native.area", geom.get(), nullptr, 0.0);
  if (value.kind != detail::IrKind::kNumber) {
    return detail::reject(error, "invalid geometry");
  }
  return detail::accept(out, detail::format_number(value.number));
}

bool length(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  int32_t crs = 0;
  if (!detail::require_epsg(crs_epsg, &crs, error)) {
    return false;
  }
  detail::OgrGeom geom = detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  if (detail::is_geographic_epsg(crs)) {
    return detail::reject(error, "geographic crs");
  }
  const detail::IrValue value = detail::eval_geometry("native.length", geom.get(), nullptr, 0.0);
  if (value.kind != detail::IrKind::kNumber) {
    return detail::reject(error, "invalid geometry");
  }
  return detail::accept(out, detail::format_number(value.number));
}

bool is_empty(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  if (!detail::require_epsg(crs_epsg, nullptr, error)) {
    return false;
  }
  detail::OgrGeom geom = detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  const detail::IrValue value = detail::eval_geometry("native.is_empty", geom.get(), nullptr, 0.0);
  if (value.kind != detail::IrKind::kFlag) {
    return detail::reject(error, "invalid geometry");
  }
  return detail::accept(out, value.flag ? "1" : "0");
}

bool is_valid(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  if (!detail::require_epsg(crs_epsg, nullptr, error)) {
    return false;
  }
  detail::OgrGeom geom = detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  const detail::IrValue value = detail::eval_geometry("native.is_valid", geom.get(), nullptr, 0.0);
  if (value.kind != detail::IrKind::kFlag) {
    return detail::reject(error, "invalid geometry");
  }
  return detail::accept(out, value.flag ? "1" : "0");
}

bool is_simple(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  if (!detail::require_epsg(crs_epsg, nullptr, error)) {
    return false;
  }
  detail::OgrGeom geom = detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  const detail::IrValue value = detail::eval_geometry("native.is_simple", geom.get(), nullptr, 0.0);
  if (value.kind != detail::IrKind::kFlag) {
    return detail::reject(error, "invalid geometry");
  }
  return detail::accept(out, value.flag ? "1" : "0");
}

bool make_valid(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  return detail::construct_shape("native.make_valid", wkt, crs_epsg, false, out, error);
}

bool densify(std::string_view wkt,
             int32_t wkt_epsg,
             std::string_view tolerance,
             int32_t tolerance_epsg,
             std::string* out,
             std::string* error) {
  return detail::construct_width("native.densify", wkt, wkt_epsg, tolerance, tolerance_epsg, out,
                                 error);
}

bool unary_union(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  return detail::construct_shape("native.unary_union", wkt, crs_epsg, false, out, error);
}

bool point_on_surface(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  return detail::construct_shape("native.point_on_surface", wkt, crs_epsg, false, out, error);
}

bool covers(std::string_view a,
            int32_t a_epsg,
            std::string_view b,
            int32_t b_epsg,
            std::string* out,
            std::string* error) {
  return detail::predicate_text("native.covers", a, a_epsg, b, b_epsg, out, error);
}

bool covered_by(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error) {
  return detail::predicate_text("native.covered_by", a, a_epsg, b, b_epsg, out, error);
}

bool hausdorff_distance(std::string_view a,
                        int32_t a_epsg,
                        std::string_view b,
                        int32_t b_epsg,
                        std::string* out,
                        std::string* error) {
  int32_t crs = 0;
  if (!detail::require_same_epsg(a_epsg, b_epsg, &crs, error)) {
    return false;
  }
  if (detail::is_geographic_epsg(crs)) {
    return detail::reject(error, "geographic crs");
  }
  detail::OgrGeom left = detail::geometry_from_wkt(a, error);
  if (!left) {
    return false;
  }
  detail::OgrGeom right = detail::geometry_from_wkt(b, error);
  if (!right) {
    return false;
  }
  const detail::IrValue value =
      detail::eval_geometry("native.hausdorff_distance", left.get(), right.get(), 0.0);
  if (value.kind != detail::IrKind::kNumber) {
    return detail::reject(error, "invalid geometry");
  }
  return detail::accept(out, detail::format_number(value.number));
}

bool delaunay(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) {
  return detail::construct_shape("native.delaunay", wkt, crs_epsg, false, out, error);
}

bool transform(std::string_view wkt,
               int32_t src_epsg,
               int32_t dst_epsg,
               std::string* out,
               std::string* error) {
  if (!detail::require_epsg(src_epsg, nullptr, error)) {
    return false;
  }
  if (dst_epsg == 0) {
    return detail::reject(error, "unknown crs");
  }
  detail::OgrGeom geom = detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  detail::IrEnv env;
  env.input = geom.get();
  env.src_epsg = src_epsg;
  env.dst_epsg = dst_epsg;
  const detail::IrValue value = detail::eval_env("native.transform", env);
  if (value.kind != detail::IrKind::kFlag) {
    return detail::reject(error, "transform");
  }
  return detail::accept(out, detail::geometry_to_wkt(geom.get()));
}

}  // namespace gis
