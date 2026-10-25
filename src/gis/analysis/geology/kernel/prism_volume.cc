// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geology/kernel/prism_volume.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "gis/geo/xy/xy.h"

namespace gis {

PrismVolumeResult prism_volume_between(const BoreholeSet& holes,
                                       std::string_view top_stratum_id,
                                       std::string_view bottom_stratum_id) {
  PrismVolumeResult out;
  out.stratum_id = std::string(top_stratum_id);
  if (!out.stratum_id.empty() && !bottom_stratum_id.empty()) {
    out.stratum_id += "/" + std::string(bottom_stratum_id);
  }

  if (!holes.ok) {
    out.error = holes.error.empty() ? "invalid borehole set" : holes.error;
    return out;
  }
  if (top_stratum_id.empty() || bottom_stratum_id.empty()) {
    out.error = "empty top/bottom stratum_id";
    return out;
  }
  if (top_stratum_id == bottom_stratum_id) {
    out.error = "top and bottom stratum_id must differ";
    return out;
  }

  struct Xyz {
    double x = 0;
    double y = 0;
    double z = 0;
  };

  std::unordered_map<std::string, Xyz> top;
  std::unordered_map<std::string, Xyz> bottom;
  for (const BoreholeContact& c : holes.contacts) {
    if (c.stratum_id == top_stratum_id) {
      top[c.hole_id] = {c.x, c.y, c.z};
    } else if (c.stratum_id == bottom_stratum_id) {
      bottom[c.hole_id] = {c.x, c.y, c.z};
    }
  }

  std::vector<geo::Xy> xy;
  double sum_thickness = 0.0;
  int n_pair = 0;
  for (const auto& [hole_id, t] : top) {
    const auto it = bottom.find(hole_id);
    if (it == bottom.end()) {
      continue;
    }
    const Xyz& b = it->second;
    const double thickness = t.z - b.z;
    if (!(thickness > 0.0)) {
      continue;
    }
    // Use mid-XY of the paired contacts (usually identical).
    xy.push_back({0.5 * (t.x + b.x), 0.5 * (t.y + b.y)});
    sum_thickness += thickness;
    ++n_pair;
  }

  if (n_pair == 0) {
    out.error = "no hole pairs with positive top-bottom thickness";
    return out;
  }

  const double avg_thickness = sum_thickness / static_cast<double>(n_pair);
  const std::vector<geo::Xy> hull = geo::convex_hull_xy(xy);
  double area = geo::polygon_area_xy(hull);
  if (!(area > 0.0)) {
    area = geo::bbox_area_xy(xy);
  }
  if (!(area > 0.0)) {
    out.error = "degenerate XY footprint";
    return out;
  }

  out.volume = avg_thickness * area;
  out.ok = true;
  return out;
}

}  // namespace gis
