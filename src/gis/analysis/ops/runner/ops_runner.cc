// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/runner/ops_runner.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "gis/analysis/ops/geojson/geojson_codec.h"
#include "gis/analysis/ops/ir/geometry_ir.h"
#include "gis/analysis/ops/runner/op_table.h"
#include "gis/analysis/ops/runner/ops_args.h"

#include "ogr_geometry.h"

namespace gis {
namespace detail {
namespace {

bool run_geometry_file_op(std::string_view processing_id, std::string_view args_json) {
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

  double dx = 0.0;
  double dy = 0.0;
  double angle = 0.0;
  double scale_x = 1.0;
  double scale_y = 1.0;
  double origin_x = 0.0;
  double origin_y = 0.0;
  (void)op_arg_double(args, "dx", &dx);
  (void)op_arg_double(args, "dy", &dy);
  (void)op_arg_double(args, "angle", &angle);
  (void)op_arg_double(args, "scale_x", &scale_x);
  (void)op_arg_double(args, "scale_y", &scale_y);
  (void)op_arg_double(args, "origin_x", &origin_x);
  (void)op_arg_double(args, "origin_y", &origin_y);
  if (scale_x == 0.0) {
    scale_x = 1.0;
  }
  if (scale_y == 0.0) {
    scale_y = 1.0;
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

  IrEnv env;
  env.input = geom.get();
  env.other = clip.get();
  env.number = distance;
  env.param_y = dy;
  env.origin_x = origin_x;
  env.origin_y = origin_y;
  if (processing_id == "native.translate") {
    env.number = dx;
    env.param_y = dy;
  } else if (processing_id == "native.rotate") {
    env.number = angle;
  } else if (processing_id == "native.scale") {
    env.number = scale_x;
    env.param_y = scale_y;
  }

  std::optional<IrValue> hit = eval_geometry_ir(processing_id, env);
  if (!hit) {
    auto result = apply_op(processing_id, geom.get(), clip.get(), distance);
    if (!result || result->IsEmpty()) {
      return false;
    }
    return geometry_to_geojson(output, result.get());
  }
  if (hit->kind == IrKind::kGeometry && hit->geometry) {
    if (hit->geometry->IsEmpty()) {
      return false;
    }
    return geometry_to_geojson(output, hit->geometry.get());
  }
  if (hit->kind == IrKind::kFlag) {
    return hit->flag;
  }
  return false;
}

}  // namespace

bool run_builtin_op(std::string_view processing_id, std::string_view args_json) {
  const BuiltinOpEntry* entry = find_builtin_op(processing_id);
  if (!entry) {
    return false;
  }
  if (entry->run) {
    return entry->run(args_json);
  }
  return run_geometry_file_op(processing_id, args_json);
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
