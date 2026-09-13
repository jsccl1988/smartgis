// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_POLYLINE_H_
#define PLUGIN_ORTHOGRID_POLYLINE_H_

#include "base/core/bas_struct.h"

#include <vector>

namespace orthogrid {
namespace detail {

// Distance from a point to the nearest polyline vertex-angle segment.
// Writes the bracketing vertex indices.
double distance_to_polyline(const base::dbfPoint& pt,
                            std::vector<base::dbfPoint>& points,
                            int& index_pre,
                            int& index_next);

// Vertex pair whose opening angle at `pt` is smallest, starting at index_pre.
long locate_on_polyline(base::dbfPoint& pt,
                        std::vector<base::dbfPoint>& points,
                        int& index_pre,
                        int& index_next);

// Foot of the perpendicular from P to line AB.
// Returns -1 (before A), 0 (on segment), 1 (beyond B), 2 (A == B).
int foot_on_segment(base::dbfPoint a,
                    base::dbfPoint b,
                    base::dbfPoint p,
                    base::dbfPoint& h);

}  // namespace detail
}  // namespace orthogrid

#endif
