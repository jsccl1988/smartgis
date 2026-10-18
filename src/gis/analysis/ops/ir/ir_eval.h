// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Feature scan / filter / project for the glang IR adapter.
// Spatial WKT is decoded through ops/wkt after the dataset opens, so an
// open failure is still reported before a bad geometry string.

#ifndef GIS_ANALYSIS_OPS_IR_EVAL_H_
#define GIS_ANALYSIS_OPS_IR_EVAL_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

GIS_EXPORT bool scan_features(std::string_view path, std::string* out, std::string* error);

GIS_EXPORT bool filter_features(std::string_view path,
                                std::string_view where,
                                std::string_view wkt,
                                std::string* out,
                                std::string* error);

GIS_EXPORT bool read_feature_field(std::string_view path,
                                   int32_t fid,
                                   std::string_view field,
                                   std::string* out,
                                   std::string* error);

GIS_EXPORT bool project_features(std::string_view path,
                                 std::string_view fields,
                                 std::string* out,
                                 std::string* error);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_IR_EVAL_H_
