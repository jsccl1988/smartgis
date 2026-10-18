// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GIS IR for the capability host. Verbs match the glang geometry / feature
// relation instructions (WKT + EPSG). Customization is `gis_traits`, not a
// second geometry hierarchy. The GDAL backend is defined next to those
// instructions; this header does not include OGR.

#ifndef CONTENT_BROWSER_CAPABILITY_GIS_H_
#define CONTENT_BROWSER_CAPABILITY_GIS_H_

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>

namespace content {

// Backend tag. `gis_traits<GdalGis>` is the OGR/GEOS implementation.
struct GdalGis {};

template <typename Backend>
struct gis_traits;

template <>
struct gis_traits<GdalGis> {
  static bool buffer(std::string_view wkt,
                     int32_t wkt_epsg,
                     std::string_view distance,
                     int32_t distance_epsg,
                     std::string* out,
                     std::string* error);
  static bool intersection(std::string_view a,
                           int32_t a_epsg,
                           std::string_view b,
                           int32_t b_epsg,
                           std::string* out,
                           std::string* error);
  static bool geom_union(std::string_view a,
                         int32_t a_epsg,
                         std::string_view b,
                         int32_t b_epsg,
                         std::string* out,
                         std::string* error);
  static bool difference(std::string_view a,
                         int32_t a_epsg,
                         std::string_view b,
                         int32_t b_epsg,
                         std::string* out,
                         std::string* error);
  static bool sym_difference(std::string_view a,
                             int32_t a_epsg,
                             std::string_view b,
                             int32_t b_epsg,
                             std::string* out,
                             std::string* error);
  static bool centroid(std::string_view wkt,
                       int32_t crs_epsg,
                       std::string* out,
                       std::string* error);
  static bool envelope(std::string_view wkt,
                       int32_t crs_epsg,
                       std::string* out,
                       std::string* error);
  static bool convex_hull(std::string_view wkt,
                          int32_t crs_epsg,
                          std::string* out,
                          std::string* error);
  static bool boundary(std::string_view wkt,
                       int32_t crs_epsg,
                       std::string* out,
                       std::string* error);
  static bool simplify(std::string_view wkt,
                       int32_t wkt_epsg,
                       std::string_view tolerance,
                       int32_t tolerance_epsg,
                       std::string* out,
                       std::string* error);
  static bool transform(std::string_view wkt,
                        int32_t src_epsg,
                        int32_t dst_epsg,
                        std::string* out,
                        std::string* error);
  static bool intersects(std::string_view a,
                         int32_t a_epsg,
                         std::string_view b,
                         int32_t b_epsg,
                         std::string* out,
                         std::string* error);
  static bool contains(std::string_view a,
                       int32_t a_epsg,
                       std::string_view b,
                       int32_t b_epsg,
                       std::string* out,
                       std::string* error);
  static bool within(std::string_view a,
                     int32_t a_epsg,
                     std::string_view b,
                     int32_t b_epsg,
                     std::string* out,
                     std::string* error);
  static bool disjoint(std::string_view a,
                       int32_t a_epsg,
                       std::string_view b,
                       int32_t b_epsg,
                       std::string* out,
                       std::string* error);
  static bool equals(std::string_view a,
                     int32_t a_epsg,
                     std::string_view b,
                     int32_t b_epsg,
                     std::string* out,
                     std::string* error);
  static bool touches(std::string_view a,
                      int32_t a_epsg,
                      std::string_view b,
                      int32_t b_epsg,
                      std::string* out,
                      std::string* error);
  static bool crosses(std::string_view a,
                      int32_t a_epsg,
                      std::string_view b,
                      int32_t b_epsg,
                      std::string* out,
                      std::string* error);
  static bool overlaps(std::string_view a,
                       int32_t a_epsg,
                       std::string_view b,
                       int32_t b_epsg,
                       std::string* out,
                       std::string* error);
  static bool distance(std::string_view a,
                       int32_t a_epsg,
                       std::string_view b,
                       int32_t b_epsg,
                       std::string* out,
                       std::string* error);
  static bool area(std::string_view wkt,
                   int32_t crs_epsg,
                   std::string* out,
                   std::string* error);
  static bool length(std::string_view wkt,
                     int32_t crs_epsg,
                     std::string* out,
                     std::string* error);
  static bool is_empty(std::string_view wkt,
                       int32_t crs_epsg,
                       std::string* out,
                       std::string* error);
  static bool scan_features(std::string_view path,
                            std::string* out,
                            std::string* error);
  static bool filter_features(std::string_view path,
                              std::string_view where,
                              std::string_view wkt,
                              std::string* out,
                              std::string* error);
  static bool read_feature_field(std::string_view path,
                                 int32_t fid,
                                 std::string_view field,
                                 std::string* out,
                                 std::string* error);
  static bool project_features(std::string_view path,
                               std::string_view fields,
                               std::string* out,
                               std::string* error);
};

template <typename Backend>
concept gis_backend = requires(std::string_view wkt,
                               std::string_view other,
                               int32_t epsg,
                               int32_t fid,
                               std::string* out,
                               std::string* error) {
  { gis_traits<Backend>::buffer(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::intersection(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::geom_union(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::difference(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::sym_difference(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::centroid(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::envelope(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::convex_hull(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::boundary(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::simplify(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::transform(wkt, epsg, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::intersects(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::contains(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::within(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::disjoint(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::equals(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::touches(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::crosses(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::overlaps(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::distance(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::area(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::length(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::is_empty(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::scan_features(wkt, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::filter_features(wkt, other, wkt, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::read_feature_field(wkt, fid, other, out, error) } -> std::convertible_to<bool>;
  { gis_traits<Backend>::project_features(wkt, other, out, error) } -> std::convertible_to<bool>;
};

// Host GIS lane. Stateless: every verb is the selected backend, not a slot
// the shell fills.
template <gis_backend Backend = GdalGis>
struct GisEngine {
  using backend_type = Backend;

  bool buffer(std::string_view wkt,
              int32_t wkt_epsg,
              std::string_view distance,
              int32_t distance_epsg,
              std::string* out,
              std::string* error) const {
    return gis_traits<Backend>::buffer(wkt, wkt_epsg, distance, distance_epsg, out, error);
  }

  bool intersection(std::string_view a,
                    int32_t a_epsg,
                    std::string_view b,
                    int32_t b_epsg,
                    std::string* out,
                    std::string* error) const {
    return gis_traits<Backend>::intersection(a, a_epsg, b, b_epsg, out, error);
  }

  bool geom_union(std::string_view a,
                  int32_t a_epsg,
                  std::string_view b,
                  int32_t b_epsg,
                  std::string* out,
                  std::string* error) const {
    return gis_traits<Backend>::geom_union(a, a_epsg, b, b_epsg, out, error);
  }

  bool difference(std::string_view a,
                  int32_t a_epsg,
                  std::string_view b,
                  int32_t b_epsg,
                  std::string* out,
                  std::string* error) const {
    return gis_traits<Backend>::difference(a, a_epsg, b, b_epsg, out, error);
  }

  bool sym_difference(std::string_view a,
                      int32_t a_epsg,
                      std::string_view b,
                      int32_t b_epsg,
                      std::string* out,
                      std::string* error) const {
    return gis_traits<Backend>::sym_difference(a, a_epsg, b, b_epsg, out, error);
  }

  bool centroid(std::string_view wkt,
                int32_t crs_epsg,
                std::string* out,
                std::string* error) const {
    return gis_traits<Backend>::centroid(wkt, crs_epsg, out, error);
  }

  bool envelope(std::string_view wkt,
                int32_t crs_epsg,
                std::string* out,
                std::string* error) const {
    return gis_traits<Backend>::envelope(wkt, crs_epsg, out, error);
  }

  bool convex_hull(std::string_view wkt,
                   int32_t crs_epsg,
                   std::string* out,
                   std::string* error) const {
    return gis_traits<Backend>::convex_hull(wkt, crs_epsg, out, error);
  }

  bool boundary(std::string_view wkt,
                int32_t crs_epsg,
                std::string* out,
                std::string* error) const {
    return gis_traits<Backend>::boundary(wkt, crs_epsg, out, error);
  }

  bool simplify(std::string_view wkt,
                int32_t wkt_epsg,
                std::string_view tolerance,
                int32_t tolerance_epsg,
                std::string* out,
                std::string* error) const {
    return gis_traits<Backend>::simplify(wkt, wkt_epsg, tolerance, tolerance_epsg, out, error);
  }

  bool transform(std::string_view wkt,
                 int32_t src_epsg,
                 int32_t dst_epsg,
                 std::string* out,
                 std::string* error) const {
    return gis_traits<Backend>::transform(wkt, src_epsg, dst_epsg, out, error);
  }

  bool intersects(std::string_view a,
                  int32_t a_epsg,
                  std::string_view b,
                  int32_t b_epsg,
                  std::string* out,
                  std::string* error) const {
    return gis_traits<Backend>::intersects(a, a_epsg, b, b_epsg, out, error);
  }

  bool contains(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error) const {
    return gis_traits<Backend>::contains(a, a_epsg, b, b_epsg, out, error);
  }

  bool within(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error) const {
    return gis_traits<Backend>::within(a, a_epsg, b, b_epsg, out, error);
  }

  bool disjoint(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error) const {
    return gis_traits<Backend>::disjoint(a, a_epsg, b, b_epsg, out, error);
  }

  bool equals(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error) const {
    return gis_traits<Backend>::equals(a, a_epsg, b, b_epsg, out, error);
  }

  bool touches(std::string_view a,
               int32_t a_epsg,
               std::string_view b,
               int32_t b_epsg,
               std::string* out,
               std::string* error) const {
    return gis_traits<Backend>::touches(a, a_epsg, b, b_epsg, out, error);
  }

  bool crosses(std::string_view a,
               int32_t a_epsg,
               std::string_view b,
               int32_t b_epsg,
               std::string* out,
               std::string* error) const {
    return gis_traits<Backend>::crosses(a, a_epsg, b, b_epsg, out, error);
  }

  bool overlaps(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error) const {
    return gis_traits<Backend>::overlaps(a, a_epsg, b, b_epsg, out, error);
  }

  bool distance(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error) const {
    return gis_traits<Backend>::distance(a, a_epsg, b, b_epsg, out, error);
  }

  bool area(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) const {
    return gis_traits<Backend>::area(wkt, crs_epsg, out, error);
  }

  bool length(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) const {
    return gis_traits<Backend>::length(wkt, crs_epsg, out, error);
  }

  bool is_empty(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error) const {
    return gis_traits<Backend>::is_empty(wkt, crs_epsg, out, error);
  }

  bool scan_features(std::string_view path, std::string* out, std::string* error) const {
    return gis_traits<Backend>::scan_features(path, out, error);
  }

  bool filter_features(std::string_view path,
                       std::string_view where,
                       std::string_view wkt,
                       std::string* out,
                       std::string* error) const {
    return gis_traits<Backend>::filter_features(path, where, wkt, out, error);
  }

  bool read_feature_field(std::string_view path,
                          int32_t fid,
                          std::string_view field,
                          std::string* out,
                          std::string* error) const {
    return gis_traits<Backend>::read_feature_field(path, fid, field, out, error);
  }

  bool project_features(std::string_view path,
                        std::string_view fields,
                        std::string* out,
                        std::string* error) const {
    return gis_traits<Backend>::project_features(path, fields, out, error);
  }
};

using GisCapability = GisEngine<GdalGis>;

template <typename T>
concept gis_capability = requires(const T& gis,
                                  std::string_view wkt,
                                  std::string_view other,
                                  int32_t epsg,
                                  int32_t fid,
                                  std::string* out,
                                  std::string* error) {
  { gis.buffer(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.intersection(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.geom_union(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.difference(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.sym_difference(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.centroid(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.envelope(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.convex_hull(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.boundary(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.simplify(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.transform(wkt, epsg, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.intersects(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.contains(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.within(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.disjoint(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.equals(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.touches(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.crosses(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.overlaps(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.distance(wkt, epsg, other, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.area(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.length(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.is_empty(wkt, epsg, out, error) } -> std::convertible_to<bool>;
  { gis.scan_features(wkt, out, error) } -> std::convertible_to<bool>;
  { gis.filter_features(wkt, other, wkt, out, error) } -> std::convertible_to<bool>;
  { gis.read_feature_field(wkt, fid, other, out, error) } -> std::convertible_to<bool>;
  { gis.project_features(wkt, other, out, error) } -> std::convertible_to<bool>;
};

static_assert(gis_backend<GdalGis>);
static_assert(gis_capability<GisCapability>);

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_GIS_H_
