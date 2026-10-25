// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layer-scale geometry ops (dissolve, clip, spatial join, erase/identity,
// split/cut) for Processing. Kernels call geo:: set/predicate/shape ops;
// vector I/O stays in this module.
//
// Attribute policy (overlay):
//   erase     — keep input fields only
//   identity  — input fields + optional identity fields prefixed id_
//               (overlap parts); outside parts keep input fields only

#ifndef GIS_ANALYSIS_LAYER_LAYER_OPS_H_
#define GIS_ANALYSIS_LAYER_LAYER_OPS_H_

#include <string>
#include <string_view>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

struct LayerOpResult {
  bool ok = false;
  int features_out = 0;
  std::string error;
};

GIS_EXPORT LayerOpResult dissolve_by_field(std::string_view input_path,
                                           std::string_view field_name,
                                           std::string_view output_path);

GIS_EXPORT LayerOpResult clip_layer_by_polygons(std::string_view input_path,
                                                std::string_view clip_layer_path,
                                                std::string_view output_path);

GIS_EXPORT LayerOpResult spatial_join_layer(std::string_view target_path,
                                            std::string_view join_path,
                                            std::string_view predicate,
                                            std::string_view copy_fields,
                                            std::string_view output_path);

GIS_EXPORT LayerOpResult erase_layer(std::string_view input_path,
                                     std::string_view erase_layer_path,
                                     std::string_view output_path);

// ArcGIS-shaped Identity: input ∩ identity (attrs from both) plus
// input − identity_union (input attrs only). Optional copy_fields from identity.
GIS_EXPORT LayerOpResult identity_layer(std::string_view input_path,
                                        std::string_view identity_path,
                                        std::string_view copy_fields,
                                        std::string_view output_path);

// Split line features at nearby points (distance along line; |d| <= tolerance).
GIS_EXPORT LayerOpResult split_line_by_points(std::string_view line_path,
                                              std::string_view point_path,
                                              double tolerance,
                                              std::string_view output_path);

// Cut polygon features with line features (node boundary+cutters → polygonize;
// keep pieces whose point-on-surface lies in the original).
GIS_EXPORT LayerOpResult cut_polygon_by_lines(std::string_view polygon_path,
                                              std::string_view line_path,
                                              std::string_view output_path);

GIS_EXPORT bool run_dissolve_op(std::string_view args_json);
GIS_EXPORT bool run_clip_layer_op(std::string_view args_json);
GIS_EXPORT bool run_spatial_join_op(std::string_view args_json);
GIS_EXPORT bool run_erase_op(std::string_view args_json);
GIS_EXPORT bool run_identity_op(std::string_view args_json);
GIS_EXPORT bool run_split_line_op(std::string_view args_json);
GIS_EXPORT bool run_cut_polygon_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_LAYER_LAYER_OPS_H_
