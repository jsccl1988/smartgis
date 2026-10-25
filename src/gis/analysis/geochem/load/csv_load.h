// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_LOAD_CSV_LOAD_H_
#define GIS_ANALYSIS_GEOCHEM_LOAD_CSV_LOAD_H_

#include <string_view>

#include "gis/analysis/geochem/model/sample.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

// CSV: lon,lat (or x,y) + optional id + one or more numeric element columns.
// Header required. Recognized lon aliases: lon, longitude, x, lng.
// Recognized lat aliases: lat, latitude, y.
GIS_EXPORT GeochemSampleSet load_csv(std::string_view path);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_LOAD_CSV_LOAD_H_
