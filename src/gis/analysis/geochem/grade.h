// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_GRADE_H_
#define GIS_ANALYSIS_GEOCHEM_GRADE_H_

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "gis/analysis/geochem/samples.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

// One legend class for graded sample / anomaly colors.
struct GeochemGradeClass {
  double lo = 0;
  double hi = 0;
  // Packed RGBA (A high byte). Default opaque.
  uint32_t rgba = 0xff0000ff;
  std::string label;
};

// Equal-interval or quantile breaks over |element| values.
struct GeochemGradeLegend {
  bool ok = false;
  std::string element;
  std::vector<GeochemGradeClass> classes;
  std::string error;
};

// Build |class_count| equal-interval classes (2..12). Colors: blue→cyan→yellow→red.
GIS_EXPORT GeochemGradeLegend build_geochem_grade_legend(
    const GeochemSampleSet& set,
    std::string_view element,
    int class_count);

// Class index for |value|, or -1 when legend empty / out of range.
GIS_EXPORT int geochem_grade_class_index(const GeochemGradeLegend& legend,
                                         double value);

// #RRGGBB for style ["get","color"] field.
GIS_EXPORT std::string geochem_rgba_to_hex(uint32_t rgba);

// Map |value| into 0..100 heat score for map2d interpolate ramps.
// When |max_v| <= |min_v|, returns 50.
GIS_EXPORT double geochem_heat_score(double value, double min_v, double max_v);

// Writes a numeric heat string for style ["get","heat"] (e.g. "42.5").
// Returns |buf| on success, empty string on bad args.
GIS_EXPORT const char* format_geochem_heat(double score,
                                           char* buf,
                                           size_t cap);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_GRADE_H_
