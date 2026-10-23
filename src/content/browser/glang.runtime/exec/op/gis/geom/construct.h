// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// WKT plus EPSG packing for geometry-IR verbs. Results are text, not OGR.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_IR_GIS_CONSTRUCT_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_IR_GIS_CONSTRUCT_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace content {
namespace code {
namespace detail {

bool construct_pair(const char* id,
                    std::string_view a,
                    int32_t a_epsg,
                    std::string_view b,
                    int32_t b_epsg,
                    std::string* out,
                    std::string* error);

bool construct_shape(const char* id,
                     std::string_view wkt,
                     int32_t crs_epsg,
                     bool reject_empty,
                     std::string* out,
                     std::string* error);

bool construct_width(const char* id,
                     std::string_view wkt,
                     int32_t wkt_epsg,
                     std::string_view distance,
                     int32_t distance_epsg,
                     std::string* out,
                     std::string* error);

// Binary spatial predicate. Writes "1" or "0".
bool predicate_text(const char* id,
                    std::string_view a,
                    int32_t a_epsg,
                    std::string_view b,
                    int32_t b_epsg,
                    std::string* out,
                    std::string* error);

}  // namespace detail
}  // namespace code
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_IR_GIS_CONSTRUCT_H_
