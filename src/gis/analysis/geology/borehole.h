// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOLOGY_BOREHOLE_H_
#define GIS_ANALYSIS_GEOLOGY_BOREHOLE_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// One stratum contact sampled in a borehole (map XY + contact elevation Z).
struct BoreholeContact {
  std::string hole_id;
  double x = 0;
  double y = 0;
  double z = 0;
  std::string stratum_id;
};

// Loaded borehole contact table (CSV or in-memory).
struct BoreholeSet {
  bool ok = false;
  std::vector<BoreholeContact> contacts;
  std::string error;
};

// CSV columns: hole_id,x,y,z,stratum_id (header required).
GIS_EXPORT BoreholeSet load_boreholes_csv(std::string_view path);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOLOGY_BOREHOLE_H_
