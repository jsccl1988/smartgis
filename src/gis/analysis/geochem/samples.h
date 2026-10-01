// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_SAMPLES_H_
#define GIS_ANALYSIS_GEOCHEM_SAMPLES_H_

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

// CSV: lon,lat (or x,y) + optional id + one or more numeric element columns.
// Header required. Recognized lon aliases: lon, longitude, x, lng.
// Recognized lat aliases: lat, latitude, y.
GIS_EXPORT GeochemSampleSet load_geochem_csv(std::string_view path);

// OGR vector points: numeric field |element| (or all numeric fields when empty).
GIS_EXPORT GeochemSampleSet load_geochem_vector(std::string_view path,
                                               std::string_view element);

// Index of |element| in set.element_names, or -1.
GIS_EXPORT int geochem_element_index(const GeochemSampleSet& set,
                                     std::string_view element);

// Extract one element's values (NaN skipped). False when element missing.
GIS_EXPORT bool geochem_values_for_element(const GeochemSampleSet& set,
                                           std::string_view element,
                                           std::vector<double>* out);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_SAMPLES_H_
