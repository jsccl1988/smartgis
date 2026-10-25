// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// WKT+EPSG geometry-IR glue: eval nodes and emit WKT. Used by wkt_ops only.

#ifndef GIS_ANALYSIS_OPS_WKT_WKT_OPS_INTERNAL_H_
#define GIS_ANALYSIS_OPS_WKT_WKT_OPS_INTERNAL_H_

#include "gis/analysis/ops/ir/geometry_ir.h"

#include <cstdint>
#include <string>
#include <string_view>

class OGRGeometry;

namespace gis {
namespace detail {

bool write_geometry(const OGRGeometry& src,
                    IrValue value,
                    std::string* out,
                    std::string* error);

IrValue eval_env(const char* id, const IrEnv& env);

IrValue eval_geometry(const char* id, OGRGeometry* input, OGRGeometry* other, double number);

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

bool predicate_text(const char* id,
                    std::string_view a,
                    int32_t a_epsg,
                    std::string_view b,
                    int32_t b_epsg,
                    std::string* out,
                    std::string* error);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_WKT_WKT_OPS_INTERNAL_H_
