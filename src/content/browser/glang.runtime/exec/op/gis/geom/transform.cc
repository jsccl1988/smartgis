// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Reproject one WKT geometry. The IR node rewrites the geometry in place
// and reports success as a flag.

#include "content/browser/glang.runtime/exec/op/gis/geom/gis.h"

#include "content/browser/glang.runtime/exec/op/gis/geom/write.h"
#include "gis/analysis/ops/ir/ir_text.h"
#include "gis/analysis/ops/wkt/wkt_codec.h"

namespace content {
namespace code {

bool transform(std::string_view wkt,
               int32_t src_epsg,
               int32_t dst_epsg,
               std::string* out,
               std::string* error) {
  if (!gis::detail::require_epsg(src_epsg, nullptr, error)) {
    return false;
  }
  if (dst_epsg == 0) {
    return gis::detail::reject(error, "unknown crs");
  }
  gis::detail::OgrGeom geom = gis::detail::geometry_from_wkt(wkt, error);
  if (!geom) {
    return false;
  }
  gis::detail::IrEnv env;
  env.input = geom.get();
  env.src_epsg = src_epsg;
  env.dst_epsg = dst_epsg;
  const gis::detail::IrValue value = detail::eval_env("native.transform", env);
  if (value.kind != gis::detail::IrKind::kFlag) {
    return gis::detail::reject(error, "transform");
  }
  return gis::detail::accept(out, gis::detail::geometry_to_wkt(geom.get()));
}

}  // namespace code
}  // namespace content
