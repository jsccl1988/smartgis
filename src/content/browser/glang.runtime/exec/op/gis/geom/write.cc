// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/gis/geom/write.h"

#include "gis/analysis/ops/ir/ir_text.h"
#include "gis/analysis/ops/wkt/wkt_codec.h"
#include "gis/geo/ops/geometry_traits.h"

#include <optional>
#include <string>
#include <utility>

namespace content {
namespace code {
namespace detail {

bool write_geometry(const OGRGeometry& src,
                    gis::detail::IrValue value,
                    std::string* out,
                    std::string* error) {
  if (value.kind != gis::detail::IrKind::kGeometry || !value.geometry) {
    return gis::detail::reject(error, "invalid geometry");
  }
  value.geometry->setCoordinateDimension(
      geo::geometry_traits<OGRGeometry>::coordinate_dimension(src));
  const std::string text = gis::detail::geometry_to_wkt(value.geometry.get());
  if (text.empty()) {
    return gis::detail::reject(error, "invalid geometry");
  }
  return gis::detail::accept(out, text);
}

gis::detail::IrValue eval_env(const char* id, const gis::detail::IrEnv& env) {
  std::optional<gis::detail::IrValue> hit = gis::detail::eval_geometry_ir(id, env);
  if (!hit) {
    return {};
  }
  return std::move(*hit);
}

gis::detail::IrValue eval_geometry(const char* id,
                                   OGRGeometry* input,
                                   OGRGeometry* other,
                                   double number) {
  gis::detail::IrEnv env;
  env.input = input;
  env.other = other;
  env.number = number;
  return eval_env(id, env);
}

}  // namespace detail
}  // namespace code
}  // namespace content
