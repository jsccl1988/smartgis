// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOLOGY_MODEL_BOREHOLE_H_
#define GIS_ANALYSIS_GEOLOGY_MODEL_BOREHOLE_H_

#include <string>
#include <vector>

namespace gis {

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

}  // namespace gis

#endif  // GIS_ANALYSIS_GEOLOGY_MODEL_BOREHOLE_H_
