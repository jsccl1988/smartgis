// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GdalGis trait specializations. They forward to content::ir and do not decode WKT.

#include "content/browser/capability/gis.h"

#include "content/browser/glang.runtime/language/ir/gis.h"

namespace content {

bool gis_traits<GdalGis>::buffer(std::string_view wkt,
                                 int32_t wkt_epsg,
                                 std::string_view distance,
                                 int32_t distance_epsg,
                                 std::string* out,
                                 std::string* error) {
  return content::ir::buffer(wkt, wkt_epsg, distance, distance_epsg, out, error);
}

bool gis_traits<GdalGis>::intersection(std::string_view a,
                                       int32_t a_epsg,
                                       std::string_view b,
                                       int32_t b_epsg,
                                       std::string* out,
                                       std::string* error) {
  return content::ir::intersection(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::geom_union(std::string_view a,
                                     int32_t a_epsg,
                                     std::string_view b,
                                     int32_t b_epsg,
                                     std::string* out,
                                     std::string* error) {
  return content::ir::geom_union(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::difference(std::string_view a,
                                     int32_t a_epsg,
                                     std::string_view b,
                                     int32_t b_epsg,
                                     std::string* out,
                                     std::string* error) {
  return content::ir::difference(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::sym_difference(std::string_view a,
                                         int32_t a_epsg,
                                         std::string_view b,
                                         int32_t b_epsg,
                                         std::string* out,
                                         std::string* error) {
  return content::ir::sym_difference(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::centroid(std::string_view wkt,
                                   int32_t crs_epsg,
                                   std::string* out,
                                   std::string* error) {
  return content::ir::centroid(wkt, crs_epsg, out, error);
}

bool gis_traits<GdalGis>::envelope(std::string_view wkt,
                                   int32_t crs_epsg,
                                   std::string* out,
                                   std::string* error) {
  return content::ir::envelope(wkt, crs_epsg, out, error);
}

bool gis_traits<GdalGis>::convex_hull(std::string_view wkt,
                                      int32_t crs_epsg,
                                      std::string* out,
                                      std::string* error) {
  return content::ir::convex_hull(wkt, crs_epsg, out, error);
}

bool gis_traits<GdalGis>::boundary(std::string_view wkt,
                                   int32_t crs_epsg,
                                   std::string* out,
                                   std::string* error) {
  return content::ir::boundary(wkt, crs_epsg, out, error);
}

bool gis_traits<GdalGis>::simplify(std::string_view wkt,
                                   int32_t wkt_epsg,
                                   std::string_view tolerance,
                                   int32_t tolerance_epsg,
                                   std::string* out,
                                   std::string* error) {
  return content::ir::simplify(wkt, wkt_epsg, tolerance, tolerance_epsg, out, error);
}

bool gis_traits<GdalGis>::transform(std::string_view wkt,
                                    int32_t src_epsg,
                                    int32_t dst_epsg,
                                    std::string* out,
                                    std::string* error) {
  return content::ir::transform(wkt, src_epsg, dst_epsg, out, error);
}

bool gis_traits<GdalGis>::intersects(std::string_view a,
                                     int32_t a_epsg,
                                     std::string_view b,
                                     int32_t b_epsg,
                                     std::string* out,
                                     std::string* error) {
  return content::ir::intersects(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::contains(std::string_view a,
                                   int32_t a_epsg,
                                   std::string_view b,
                                   int32_t b_epsg,
                                   std::string* out,
                                   std::string* error) {
  return content::ir::contains(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::within(std::string_view a,
                                 int32_t a_epsg,
                                 std::string_view b,
                                 int32_t b_epsg,
                                 std::string* out,
                                 std::string* error) {
  return content::ir::within(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::disjoint(std::string_view a,
                                   int32_t a_epsg,
                                   std::string_view b,
                                   int32_t b_epsg,
                                   std::string* out,
                                   std::string* error) {
  return content::ir::disjoint(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::equals(std::string_view a,
                                 int32_t a_epsg,
                                 std::string_view b,
                                 int32_t b_epsg,
                                 std::string* out,
                                 std::string* error) {
  return content::ir::equals(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::touches(std::string_view a,
                                  int32_t a_epsg,
                                  std::string_view b,
                                  int32_t b_epsg,
                                  std::string* out,
                                  std::string* error) {
  return content::ir::touches(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::crosses(std::string_view a,
                                  int32_t a_epsg,
                                  std::string_view b,
                                  int32_t b_epsg,
                                  std::string* out,
                                  std::string* error) {
  return content::ir::crosses(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::overlaps(std::string_view a,
                                   int32_t a_epsg,
                                   std::string_view b,
                                   int32_t b_epsg,
                                   std::string* out,
                                   std::string* error) {
  return content::ir::overlaps(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::distance(std::string_view a,
                                   int32_t a_epsg,
                                   std::string_view b,
                                   int32_t b_epsg,
                                   std::string* out,
                                   std::string* error) {
  return content::ir::distance(a, a_epsg, b, b_epsg, out, error);
}

bool gis_traits<GdalGis>::area(std::string_view wkt,
                               int32_t crs_epsg,
                               std::string* out,
                               std::string* error) {
  return content::ir::area(wkt, crs_epsg, out, error);
}

bool gis_traits<GdalGis>::length(std::string_view wkt,
                                 int32_t crs_epsg,
                                 std::string* out,
                                 std::string* error) {
  return content::ir::length(wkt, crs_epsg, out, error);
}

bool gis_traits<GdalGis>::is_empty(std::string_view wkt,
                                   int32_t crs_epsg,
                                   std::string* out,
                                   std::string* error) {
  return content::ir::is_empty(wkt, crs_epsg, out, error);
}

bool gis_traits<GdalGis>::scan_features(std::string_view path,
                                        std::string* out,
                                        std::string* error) {
  return content::ir::scan_features(path, out, error);
}

bool gis_traits<GdalGis>::filter_features(std::string_view path,
                                          std::string_view where,
                                          std::string_view wkt,
                                          std::string* out,
                                          std::string* error) {
  return content::ir::filter_features(path, where, wkt, out, error);
}

bool gis_traits<GdalGis>::read_feature_field(std::string_view path,
                                             int32_t fid,
                                             std::string_view field,
                                             std::string* out,
                                             std::string* error) {
  return content::ir::read_feature_field(path, fid, field, out, error);
}

bool gis_traits<GdalGis>::project_features(std::string_view path,
                                           std::string_view fields,
                                           std::string* out,
                                           std::string* error) {
  return content::ir::project_features(path, fields, out, error);
}

}  // namespace content
