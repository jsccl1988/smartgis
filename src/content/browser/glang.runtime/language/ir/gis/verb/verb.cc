// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Thin geometry verbs. Each name is one geometry-IR id.

#include "content/browser/glang.runtime/language/ir/gis.h"

#include "content/browser/glang.runtime/language/ir/gis/construct/construct.h"

namespace content {
namespace ir {

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
  return detail::construct_width("native.simplify", wkt, wkt_epsg, tolerance, tolerance_epsg, out,
                                 error);
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

}  // namespace ir
}  // namespace content
