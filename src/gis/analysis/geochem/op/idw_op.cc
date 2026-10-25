// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/op/idw_op.h"

#include <limits>
#include <string>

#include <rapidjson/document.h>

#include "gis/analysis/geochem/idw/idw.h"
#include "gis/analysis/geochem/io/geotiff.h"
#include "gis/analysis/geochem/load/csv_load.h"
#include "gis/analysis/ops/runner/ops_args.h"

namespace gis {
namespace detail {

bool run_idw_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string element;
  std::string output;
  if (!op_arg_string(args, "input", &input) ||
      !op_arg_string(args, "element", &element) ||
      !op_arg_string(args, "output", &output)) {
    return false;
  }
  int cells = 64;
  double power = 2.0;
  double k_sigma = 2.0;
  op_arg_int(args, "cells", &cells);
  op_arg_double(args, "power", &power);
  op_arg_double(args, "k_sigma", &k_sigma);
  double threshold = std::numeric_limits<double>::quiet_NaN();
  op_arg_double(args, "threshold", &threshold);
  std::string mask_out;
  op_arg_string(args, "mask_output", &mask_out);

  GeochemSampleSet set = load_csv(input);
  if (!set.ok) {
    return false;
  }
  const GeochemIdwResult result =
      run_idw(set, element, cells, power, threshold, k_sigma);
  if (!result.ok) {
    return false;
  }
  return write_idw_geotiff(output, result, mask_out);
}

}  // namespace detail
}  // namespace gis
