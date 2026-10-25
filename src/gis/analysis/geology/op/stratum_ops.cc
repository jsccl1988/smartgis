// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geology/op/stratum_ops.h"

#include <cstdint>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "gis/analysis/geology/io/borehole_csv.h"
#include "gis/analysis/geology/kernel/prism_volume.h"
#include "gis/analysis/geology/kernel/stratum_tin.h"
#include "gis/analysis/geology/op/op_json.h"

namespace gis {

bool run_stratum_interpolate_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!detail::parse_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string stratum_id;
  std::string output;
  if (!detail::json_get_string(args, "input", &input) ||
      !detail::json_get_string(args, "stratum_id", &stratum_id) ||
      !detail::json_get_string(args, "output", &output)) {
    return false;
  }
  const BoreholeSet holes = load_boreholes_csv(input);
  const StratumTin tin = interpolate_stratum_tin(holes, stratum_id);
  if (!tin.ok) {
    return false;
  }

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("stratum_id");
  w.String(tin.stratum_id.c_str());
  w.Key("xyz");
  w.StartArray();
  for (double v : tin.xyz) {
    w.Double(v);
  }
  w.EndArray();
  w.Key("indices");
  w.StartArray();
  for (uint32_t i : tin.indices) {
    w.Uint(i);
  }
  w.EndArray();
  w.EndObject();
  return detail::write_text_file(output, buf.GetString());
}

bool run_stratum_prism_volume_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!detail::parse_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string top_id;
  std::string bottom_id;
  std::string output;
  if (!detail::json_get_string(args, "input", &input) ||
      !detail::json_get_string(args, "top_stratum_id", &top_id) ||
      !detail::json_get_string(args, "bottom_stratum_id", &bottom_id) ||
      !detail::json_get_string(args, "output", &output)) {
    return false;
  }
  const BoreholeSet holes = load_boreholes_csv(input);
  const PrismVolumeResult vol =
      prism_volume_between(holes, top_id, bottom_id);
  if (!vol.ok) {
    return false;
  }

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("stratum_id");
  w.String(vol.stratum_id.c_str());
  w.Key("volume");
  w.Double(vol.volume);
  w.EndObject();
  return detail::write_text_file(output, buf.GetString());
}

}  // namespace gis
