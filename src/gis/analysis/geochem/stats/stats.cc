// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/stats/stats.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

namespace gis {
namespace detail {
namespace {

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

GeochemElementStats compute_stats(const GeochemSampleSet& set,
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
  if (!values_for_element(set, element, &vals)) {
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
  const double sum = std::accumulate(vals.begin(), vals.end(), 0.0);
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
  const double width = span > 1e-18 ? span / static_cast<double>(bins) : 1.0;
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

GeochemCorrelation compute_correlation(const GeochemSampleSet& set,
                                       std::string_view element_a,
                                       std::string_view element_b) {
  GeochemCorrelation out;
  out.element_a = std::string(element_a);
  out.element_b = std::string(element_b);
  const int ia = element_index(set, element_a);
  const int ib = element_index(set, element_b);
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

}  // namespace detail
}  // namespace gis
