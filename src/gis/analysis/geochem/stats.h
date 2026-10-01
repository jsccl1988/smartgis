// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_STATS_H_
#define GIS_ANALYSIS_GEOCHEM_STATS_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/analysis/geochem/samples.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

// Histogram bin for one element.
struct GeochemHistBin {
  double lo = 0;
  double hi = 0;
  int count = 0;
};

// Background / threshold summary for one element.
struct GeochemElementStats {
  bool ok = false;
  std::string element;
  int count = 0;
  double min_v = 0;
  double max_v = 0;
  double mean = 0;
  double stddev = 0;
  // Robust background ≈ median; threshold = background + k * MAD (or mean+k*sd).
  double background = 0;
  double threshold = 0;
  double mad = 0;
  std::vector<GeochemHistBin> histogram;
  std::string error;
};

// Pearson correlation between two elements (complete cases only).
struct GeochemCorrelation {
  bool ok = false;
  std::string element_a;
  std::string element_b;
  int count = 0;
  double r = 0;
  std::string error;
};

// Compute stats for |element|. |bins| default 10; |k_sigma| threshold multiplier.
GIS_EXPORT GeochemElementStats compute_geochem_stats(
    const GeochemSampleSet& set,
    std::string_view element,
    int bins,
    double k_sigma);

GIS_EXPORT GeochemCorrelation compute_geochem_correlation(
    const GeochemSampleSet& set,
    std::string_view element_a,
    std::string_view element_b);

// JSON: input (CSV), element, optional bins, k_sigma, output (JSON stats).
GIS_EXPORT bool run_geochem_stats_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_STATS_H_
