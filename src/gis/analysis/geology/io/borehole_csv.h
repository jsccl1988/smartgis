// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOLOGY_IO_BOREHOLE_CSV_H_
#define GIS_ANALYSIS_GEOLOGY_IO_BOREHOLE_CSV_H_

#include <string_view>

#include "gis/analysis/geology/model/borehole.h"
#include "gis/gis_export.h"

namespace gis {

// CSV columns: hole_id,x,y,z,stratum_id (header required).
GIS_EXPORT BoreholeSet load_boreholes_csv(std::string_view path);

}  // namespace gis

#endif  // GIS_ANALYSIS_GEOLOGY_IO_BOREHOLE_CSV_H_
