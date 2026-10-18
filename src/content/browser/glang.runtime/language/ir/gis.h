// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GdalGis backend for content::gis_traits. The host lane is
// content::GisCapability. Inputs are WKT plus crs_epsg,
// matching opcode operands. On failure *error is a short reason
// ("invalid geometry", "geographic crs", "bad wkt"); the opcode layer adds
// the verb. Geographic CRS rejects length, area, distance, buffer, and
// simplify. No GEOS or OGR type appears in this header. coordinateDimension
// stays on the geometry value. The union verb is geom_union (`union` is a
// C++ keyword).

#ifndef IL_RUNTIME_IR_GIS_H_
#define IL_RUNTIME_IR_GIS_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace content {
namespace ir {

bool buffer(std::string_view wkt,
            int32_t wkt_epsg,
            std::string_view distance,
            int32_t distance_epsg,
            std::string* out,
            std::string* error);

bool intersection(std::string_view a,
                  int32_t a_epsg,
                  std::string_view b,
                  int32_t b_epsg,
                  std::string* out,
                  std::string* error);

bool geom_union(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error);

bool difference(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error);

bool sym_difference(std::string_view a,
                    int32_t a_epsg,
                    std::string_view b,
                    int32_t b_epsg,
                    std::string* out,
                    std::string* error);

bool centroid(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

bool envelope(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

bool convex_hull(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

bool boundary(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

bool simplify(std::string_view wkt,
              int32_t wkt_epsg,
              std::string_view tolerance,
              int32_t tolerance_epsg,
              std::string* out,
              std::string* error);

bool transform(std::string_view wkt,
               int32_t src_epsg,
               int32_t dst_epsg,
               std::string* out,
               std::string* error);

bool intersects(std::string_view a,
                int32_t a_epsg,
                std::string_view b,
                int32_t b_epsg,
                std::string* out,
                std::string* error);

bool contains(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error);

bool within(std::string_view a,
            int32_t a_epsg,
            std::string_view b,
            int32_t b_epsg,
            std::string* out,
            std::string* error);

bool disjoint(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error);

bool equals(std::string_view a,
            int32_t a_epsg,
            std::string_view b,
            int32_t b_epsg,
            std::string* out,
            std::string* error);

bool touches(std::string_view a,
             int32_t a_epsg,
             std::string_view b,
             int32_t b_epsg,
             std::string* out,
             std::string* error);

bool crosses(std::string_view a,
             int32_t a_epsg,
             std::string_view b,
             int32_t b_epsg,
             std::string* out,
             std::string* error);

bool overlaps(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error);

bool distance(std::string_view a,
              int32_t a_epsg,
              std::string_view b,
              int32_t b_epsg,
              std::string* out,
              std::string* error);

bool area(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

bool length(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

bool is_empty(std::string_view wkt, int32_t crs_epsg, std::string* out, std::string* error);

// Feature scan over a vector dataset path. Results are JSON text.
// A missing path, unreadable dataset, or unknown field fails with a string.

bool scan_features(std::string_view path, std::string* out, std::string* error);

bool filter_features(std::string_view path,
                     std::string_view where,
                     std::string_view wkt,
                     std::string* out,
                     std::string* error);

bool read_feature_field(std::string_view path,
                        int32_t fid,
                        std::string_view field,
                        std::string* out,
                        std::string* error);

bool project_features(std::string_view path,
                      std::string_view fields,
                      std::string* out,
                      std::string* error);

}  // namespace ir
}  // namespace content

#endif  // IL_RUNTIME_IR_GIS_H_
