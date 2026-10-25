// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// One geometry IR. Each op is a node with a stable id. File runners and the
// glang boundary both evaluate through eval_geometry_ir. Codecs stay outside.

#ifndef GIS_ANALYSIS_OPS_IR_GEOMETRY_IR_H_
#define GIS_ANALYSIS_OPS_IR_GEOMETRY_IR_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "gis/analysis/ops/ir/ogr_geom.h"
#include "gis/gis_export.h"

class OGRGeometry;

namespace gis {
namespace detail {

enum class IrKind { kNone, kGeometry, kNumber, kFlag, kText };

// Inputs already decoded to OGR geometries and numbers.
struct IrEnv {
  OGRGeometry* input = nullptr;
  OGRGeometry* other = nullptr;
  double number = 0;
  double param_y = 0;
  double origin_x = 0;
  double origin_y = 0;
  int32_t src_epsg = 0;
  int32_t dst_epsg = 0;
};

// Geometry results are owned. Number / flag / text leave geometry empty.
struct IrValue {
  IrKind kind = IrKind::kNone;
  OgrGeom geometry;
  double number = 0;
  bool flag = false;
  std::string text;
};

// nullopt means the id is not a geometry IR node.
GIS_EXPORT std::optional<IrValue> eval_geometry_ir(std::string_view id, IrEnv env);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_IR_GEOMETRY_IR_H_
