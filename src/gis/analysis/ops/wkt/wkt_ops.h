// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// WKT plus EPSG spatial ops for product and script backends. Inputs are WKT
// strings and CRS codes; outputs are WKT or scalar text ("1"/"0"/number).
// On failure *error is a short reason ("invalid geometry", "geographic crs",
// "bad wkt"). Geographic CRS rejects length, area, distance, buffer, and
// simplify. The union verb is geom_union (`union` is a C++ keyword).

#ifndef GIS_ANALYSIS_OPS_WKT_WKT_OPS_H_
#define GIS_ANALYSIS_OPS_WKT_WKT_OPS_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "gis/gis_export.h"

namespace gis {

GIS_EXPORT bool buffer(std::string_view wkt,
                       int32_t wkt_epsg,
                       std::string_view distance,
                       int32_t distance_epsg,
                       std::string* out,
                       std::string* error);

GIS_EXPORT bool intersection(std::string_view a,
                             int32_t a_epsg,
                             std::string_view b,
                             int32_t b_epsg,
                             std::string* out,
                             std::string* error);

GIS_EXPORT bool geom_union(std::string_view a,
                           int32_t a_epsg,
                           std::string_view b,
                           int32_t b_epsg,
                           std::string* out,
                           std::string* error);

GIS_EXPORT bool difference(std::string_view a,
                           int32_t a_epsg,
                           std::string_view b,
                           int32_t b_epsg,
                           std::string* out,
                           std::string* error);

GIS_EXPORT bool sym_difference(std::string_view a,
                               int32_t a_epsg,
                               std::string_view b,
                               int32_t b_epsg,
                               std::string* out,
                               std::string* error);

GIS_EXPORT bool centroid(std::string_view wkt,
                         int32_t crs_epsg,
                         std::string* out,
                         std::string* error);

GIS_EXPORT bool envelope(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

GIS_EXPORT bool convex_hull(std::string_view wkt,
                            int32_t crs_epsg,
                            std::string* out,
                            std::string* error);

GIS_EXPORT bool boundary(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

GIS_EXPORT bool simplify(std::string_view wkt,
                         int32_t wkt_epsg,
                         std::string_view tolerance,
                         int32_t tolerance_epsg,
                         std::string* out,
                         std::string* error);

GIS_EXPORT bool transform(std::string_view wkt,
                          int32_t src_epsg,
                          int32_t dst_epsg,
                          std::string* out,
                          std::string* error);

GIS_EXPORT bool intersects(std::string_view a,
                           int32_t a_epsg,
                           std::string_view b,
                           int32_t b_epsg,
                           std::string* out,
                           std::string* error);

GIS_EXPORT bool contains(std::string_view a,
                         int32_t a_epsg,
                         std::string_view b,
                         int32_t b_epsg,
                         std::string* out,
                         std::string* error);

GIS_EXPORT bool within(std::string_view a,
                       int32_t a_epsg,
                       std::string_view b,
                       int32_t b_epsg,
                       std::string* out,
                       std::string* error);

GIS_EXPORT bool disjoint(std::string_view a,
                         int32_t a_epsg,
                         std::string_view b,
                         int32_t b_epsg,
                         std::string* out,
                         std::string* error);

GIS_EXPORT bool equals(std::string_view a,
                       int32_t a_epsg,
                       std::string_view b,
                       int32_t b_epsg,
                       std::string* out,
                       std::string* error);

GIS_EXPORT bool touches(std::string_view a,
                        int32_t a_epsg,
                        std::string_view b,
                        int32_t b_epsg,
                        std::string* out,
                        std::string* error);

GIS_EXPORT bool crosses(std::string_view a,
                        int32_t a_epsg,
                        std::string_view b,
                        int32_t b_epsg,
                        std::string* out,
                        std::string* error);

GIS_EXPORT bool overlaps(std::string_view a,
                         int32_t a_epsg,
                         std::string_view b,
                         int32_t b_epsg,
                         std::string* out,
                         std::string* error);

GIS_EXPORT bool distance(std::string_view a,
                         int32_t a_epsg,
                         std::string_view b,
                         int32_t b_epsg,
                         std::string* out,
                         std::string* error);

GIS_EXPORT bool area(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

GIS_EXPORT bool length(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

GIS_EXPORT bool is_empty(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

GIS_EXPORT bool is_valid(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

GIS_EXPORT bool is_simple(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

GIS_EXPORT bool make_valid(std::string_view wkt,
                           int32_t crs_epsg,
                           std::string* out,
                           std::string* error);

GIS_EXPORT bool densify(std::string_view wkt,
                        int32_t wkt_epsg,
                        std::string_view tolerance,
                        int32_t tolerance_epsg,
                        std::string* out,
                        std::string* error);

GIS_EXPORT bool unary_union(std::string_view wkt,
                            int32_t crs_epsg,
                            std::string* out,
                            std::string* error);

GIS_EXPORT bool point_on_surface(std::string_view wkt,
                                 int32_t crs_epsg,
                                 std::string* out,
                                 std::string* error);

GIS_EXPORT bool covers(std::string_view a,
                       int32_t a_epsg,
                       std::string_view b,
                       int32_t b_epsg,
                       std::string* out,
                       std::string* error);

GIS_EXPORT bool covered_by(std::string_view a,
                           int32_t a_epsg,
                           std::string_view b,
                           int32_t b_epsg,
                           std::string* out,
                           std::string* error);

GIS_EXPORT bool hausdorff_distance(std::string_view a,
                                   int32_t a_epsg,
                                   std::string_view b,
                                   int32_t b_epsg,
                                   std::string* out,
                                   std::string* error);

GIS_EXPORT bool delaunay(std::string_view wkt,
                         int32_t crs_epsg,
                         std::string* out,
                         std::string* error);

}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_WKT_WKT_OPS_H_
