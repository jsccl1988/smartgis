// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/op/stats_op.h"

#include <fstream>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "gis/analysis/geochem/load/csv_load.h"
#include "gis/analysis/geochem/stats/stats.h"
#include "gis/analysis/ops/runner/ops_args.h"

namespace gis {
namespace detail {

bool run_stats_op(std::string_view args_json) {
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
  int bins = 10;
  double k_sigma = 2.0;
  op_arg_int(args, "bins", &bins);
  op_arg_double(args, "k_sigma", &k_sigma);

  GeochemSampleSet set = load_csv(input);
  if (!set.ok) {
    return false;
  }
  const GeochemElementStats st = compute_stats(set, element, bins, k_sigma);
  if (!st.ok) {
    return false;
  }

  std::string corr_b;
  GeochemCorrelation corr;
  if (op_arg_string(args, "correlate", &corr_b) && !corr_b.empty()) {
    corr = compute_correlation(set, element, corr_b);
  }

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("ok");
  w.Bool(true);
  w.Key("element");
  w.String(st.element.c_str());
  w.Key("count");
  w.Int(st.count);
  w.Key("min");
  w.Double(st.min_v);
  w.Key("max");
  w.Double(st.max_v);
  w.Key("mean");
  w.Double(st.mean);
  w.Key("stddev");
  w.Double(st.stddev);
  w.Key("background");
  w.Double(st.background);
  w.Key("threshold");
  w.Double(st.threshold);
  w.Key("mad");
  w.Double(st.mad);
  w.Key("histogram");
  w.StartArray();
  for (const GeochemHistBin& b : st.histogram) {
    w.StartObject();
    w.Key("lo");
    w.Double(b.lo);
    w.Key("hi");
    w.Double(b.hi);
    w.Key("count");
    w.Int(b.count);
    w.EndObject();
  }
  w.EndArray();
  if (corr.ok) {
    w.Key("correlation");
    w.StartObject();
    w.Key("with");
    w.String(corr.element_b.c_str());
    w.Key("r");
    w.Double(corr.r);
    w.Key("count");
    w.Int(corr.count);
    w.EndObject();
  }
  w.EndObject();

  std::ofstream out(output, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out.write(buf.GetString(), static_cast<std::streamsize>(buf.GetSize()));
  return out.good();
}

}  // namespace detail
}  // namespace gis
