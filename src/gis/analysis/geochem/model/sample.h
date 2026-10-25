// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_MODEL_SAMPLE_H_
#define GIS_ANALYSIS_GEOCHEM_MODEL_SAMPLE_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// One geochemical sample point (map XY + multi-element concentrations).
struct GeochemSample {
  std::string id;
  double x = 0;
  double y = 0;
  // Parallel to GeochemSampleSet::element_names.
  std::vector<double> values;
};

// Loaded sample table (CSV or OGR points).
struct GeochemSampleSet {
  bool ok = false;
  std::vector<std::string> element_names;
  std::vector<GeochemSample> samples;
  std::string error;
};

// Index of |element| in set.element_names, or -1.
GIS_EXPORT int element_index(const GeochemSampleSet& set,
                             std::string_view element);

// Extract one element's values (NaN skipped). False when element missing.
GIS_EXPORT bool values_for_element(const GeochemSampleSet& set,
                                   std::string_view element,
                                   std::vector<double>* out);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_MODEL_SAMPLE_H_
