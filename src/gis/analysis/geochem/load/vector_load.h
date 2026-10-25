// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_LOAD_VECTOR_LOAD_H_
#define GIS_ANALYSIS_GEOCHEM_LOAD_VECTOR_LOAD_H_

#include <string_view>

#include "gis/analysis/geochem/model/sample.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

// OGR vector points: numeric field |element| (or all numeric fields when empty).
GIS_EXPORT GeochemSampleSet load_vector(std::string_view path,
                                        std::string_view element);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_LOAD_VECTOR_LOAD_H_
