// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/stats.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace gis {
namespace detail {
namespace {

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out || json.empty()) {
    return false;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
}

bool json_get_string(const rapidjson::Value& obj,
                     const char* key,
                     std::string* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return !out->empty();
}

bool json_get_double(const rapidjson::Value& obj, const char* key, double* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetDouble();
  return true;
}

bool json_get_int(const rapidjson::Value& obj, const char* key, int* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetInt();
  return true;
}

double median_sorted(std::vector<double> v) {
  if (v.empty()) {
    return 0;
  }
  std::sort(v.begin(), v.end());
  const size_t n = v.size();
  if (n % 2 == 1) {
    return v[n / 2];
  }
  return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

}  // namespace

GeochemElementStats compute_geochem_stats(const GeochemSampleSet& set,
                                          std::string_view element,
                                          int bins,
                                          double k_sigma) {
  GeochemElementStats out;
  out.element = std::string(element);
  if (!set.ok) {
    out.error = set.error.empty() ? "bad_samples" : set.error;
    return out;
  }
  std::vector<double> vals;
  if (!geochem_values_for_element(set, element, &vals)) {
    out.error = "element_missing";
    return out;
  }
  if (bins < 2) {
    bins = 10;
  }
  if (!(k_sigma > 0.0)) {
    k_sigma = 2.0;
  }

  out.count = static_cast<int>(vals.size());
  out.min_v = *std::min_element(vals.begin(), vals.end());
  out.max_v = *std::max_element(vals.begin(), vals.end());
  const double sum =
      std::accumulate(vals.begin(), vals.end(), 0.0);
  out.mean = sum / static_cast<double>(vals.size());
  double var = 0;
  for (double v : vals) {
    const double d = v - out.mean;
    var += d * d;
  }
  out.stddev =
      std::sqrt(var / static_cast<double>(std::max<size_t>(1, vals.size() - 1)));

  out.background = median_sorted(vals);
  std::vector<double> abs_dev;
  abs_dev.reserve(vals.size());
  for (double v : vals) {
    abs_dev.push_back(std::fabs(v - out.background));
  }
  out.mad = median_sorted(std::move(abs_dev));
  // MAD→σ scale ≈ 1.4826; fall back to mean+k*sd when MAD is tiny.
  const double robust_sigma = 1.4826 * out.mad;
  if (robust_sigma > 1e-12) {
    out.threshold = out.background + k_sigma * robust_sigma;
  } else {
    out.threshold = out.mean + k_sigma * out.stddev;
  }

  const double span = out.max_v - out.min_v;
  const double width =
      span > 1e-18 ? span / static_cast<double>(bins) : 1.0;
  out.histogram.resize(static_cast<size_t>(bins));
  for (int i = 0; i < bins; ++i) {
    out.histogram[static_cast<size_t>(i)].lo =
        out.min_v + width * static_cast<double>(i);
    out.histogram[static_cast<size_t>(i)].hi =
        out.min_v + width * static_cast<double>(i + 1);
  }
  out.histogram.back().hi = out.max_v;
  for (double v : vals) {
    int bi = static_cast<int>((v - out.min_v) / width);
    if (bi < 0) {
      bi = 0;
    }
    if (bi >= bins) {
      bi = bins - 1;
    }
    out.histogram[static_cast<size_t>(bi)].count += 1;
  }
  out.ok = true;
  return out;
}

GeochemCorrelation compute_geochem_correlation(const GeochemSampleSet& set,
                                               std::string_view element_a,
                                               std::string_view element_b) {
  GeochemCorrelation out;
  out.element_a = std::string(element_a);
  out.element_b = std::string(element_b);
  const int ia = geochem_element_index(set, element_a);
  const int ib = geochem_element_index(set, element_b);
  if (ia < 0 || ib < 0) {
    out.error = "element_missing";
    return out;
  }
  std::vector<double> xa;
  std::vector<double> xb;
  for (const GeochemSample& s : set.samples) {
    if (static_cast<size_t>(ia) >= s.values.size() ||
        static_cast<size_t>(ib) >= s.values.size()) {
      continue;
    }
    const double a = s.values[static_cast<size_t>(ia)];
    const double b = s.values[static_cast<size_t>(ib)];
    if (!std::isfinite(a) || !std::isfinite(b)) {
      continue;
    }
    xa.push_back(a);
    xb.push_back(b);
  }
  if (xa.size() < 3) {
    out.error = "too_few_pairs";
    return out;
  }
  out.count = static_cast<int>(xa.size());
  const double ma =
      std::accumulate(xa.begin(), xa.end(), 0.0) / static_cast<double>(xa.size());
  const double mb =
      std::accumulate(xb.begin(), xb.end(), 0.0) / static_cast<double>(xb.size());
  double num = 0;
  double da = 0;
  double db = 0;
  for (size_t i = 0; i < xa.size(); ++i) {
    const double ua = xa[i] - ma;
    const double ub = xb[i] - mb;
    num += ua * ub;
    da += ua * ua;
    db += ub * ub;
  }
  const double den = std::sqrt(da * db);
  if (!(den > 1e-18)) {
    out.error = "zero_variance";
    return out;
  }
  out.r = num / den;
  out.ok = true;
  return out;
}

bool run_geochem_stats_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string element;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "element", &element) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  int bins = 10;
  double k_sigma = 2.0;
  json_get_int(args, "bins", &bins);
  json_get_double(args, "k_sigma", &k_sigma);

  GeochemSampleSet set = load_geochem_csv(input);
  if (!set.ok) {
    return false;
  }
  const GeochemElementStats st =
      compute_geochem_stats(set, element, bins, k_sigma);
  if (!st.ok) {
    return false;
  }

  std::string corr_b;
  GeochemCorrelation corr;
  if (json_get_string(args, "correlate", &corr_b) && !corr_b.empty()) {
    corr = compute_geochem_correlation(set, element, corr_b);
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
  out.write(buf.GetString(),
            static_cast<std::streamsize>(buf.GetSize()));
  return out.good();
}

}  // namespace detail
}  // namespace gis
