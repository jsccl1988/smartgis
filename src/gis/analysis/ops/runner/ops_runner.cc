// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/runner/ops_runner.h"

#include <memory>
#include <string>
#include <string_view>

#include "gis/analysis/geochem/idw.h"
#include "gis/analysis/geochem/stats.h"
#include "gis/analysis/geology/prism_volume.h"
#include "gis/analysis/geology/stratum_tin.h"
#include "gis/analysis/geometry/fit.h"
#include "gis/analysis/network/cost_path.h"
#include "gis/analysis/ops/geojson/geojson_codec.h"
#include "gis/analysis/ops/ir/geometry_ir.h"
#include "gis/analysis/ops/runner/ops_args.h"
#include "gis/analysis/raster/dem/dem_gradient.h"
#include "gis/analysis/raster/dem/flood_fill.h"
#include "gis/analysis/raster/dem/storm_surge.h"
#include "gis/analysis/raster/dem/storm_surge_stats.h"
#include "gis/analysis/raster/filter/raster_convolve.h"
#include "gis/analysis/raster/filter/raster_smooth.h"

#include "ogr_geometry.h"

namespace gis {
namespace detail {

bool run_builtin_op(std::string_view processing_id, std::string_view args_json) {
  if (processing_id == "native.cost_path") {
    return run_cost_path_op(args_json);
  }
  if (processing_id == "native.flood_fill") {
    return run_flood_fill_op(args_json);
  }
  if (processing_id == "native.storm_surge") {
    return run_storm_surge_op(args_json);
  }
  if (processing_id == "native.storm_surge_stats") {
    return run_storm_surge_stats_op(args_json);
  }
  if (processing_id == "native.fit_line") {
    return run_fit_line_op(args_json);
  }
  if (processing_id == "native.fit_plane") {
    return run_fit_plane_op(args_json);
  }
  if (processing_id == "native.affine_align") {
    return run_affine_align_op(args_json);
  }
  if (processing_id == "native.dem_gradient") {
    return run_dem_gradient_op(args_json);
  }
  if (processing_id == "native.raster_convolve") {
    return run_raster_convolve_op(args_json);
  }
  if (processing_id == "native.raster_smooth") {
    return run_raster_smooth_op(args_json);
  }
  if (processing_id == "native.stratum_interpolate") {
    return run_stratum_interpolate_op(args_json);
  }
  if (processing_id == "native.stratum_prism_volume") {
    return run_stratum_prism_volume_op(args_json);
  }
  if (processing_id == "native.geochem_stats") {
    return run_geochem_stats_op(args_json);
  }
  if (processing_id == "native.geochem_idw") {
    return run_geochem_idw_op(args_json);
  }

  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  if (!op_arg_string(args, "input", &input) || !op_arg_string(args, "output", &output)) {
    return false;
  }
  double distance = 0.01;
  (void)op_arg_double(args, "distance", &distance);
  if (!(distance > 0.0) &&
      (processing_id == "native.buffer" || processing_id == "native.simplify")) {
    distance = 0.01;
  }

  auto geom = geometry_from_geojson(input);
  if (!geom) {
    return false;
  }

  std::unique_ptr<OGRGeometry> clip;
  std::string clip_path;
  if (op_arg_string(args, "clip", &clip_path)) {
    clip = geometry_from_geojson(clip_path);
  }

  const bool needs_clip = processing_id == "native.clip" ||
                          processing_id == "native.intersection" ||
                          processing_id == "native.union" ||
                          processing_id == "native.difference" ||
                          processing_id == "native.symmetric_difference";
  if (needs_clip && !clip) {
    return false;
  }

  auto result = apply_op(processing_id, geom.get(), clip.get(), distance);
  if (!result || result->IsEmpty()) {
    return false;
  }
  return geometry_to_geojson(output, result.get());
}

std::unique_ptr<OGRGeometry> apply_op(std::string_view id,
                                      OGRGeometry* input,
                                      OGRGeometry* clip,
                                      double distance) {
  if (!input) {
    return nullptr;
  }
  IrEnv env;
  env.input = input;
  env.other = clip;
  env.number = distance;
  std::optional<IrValue> hit = eval_geometry_ir(id, env);
  if (!hit || hit->kind != IrKind::kGeometry || !hit->geometry) {
    return nullptr;
  }
  return std::unique_ptr<OGRGeometry>(hit->geometry.release());
}

}  // namespace detail
}  // namespace gis
