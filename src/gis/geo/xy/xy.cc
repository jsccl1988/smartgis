// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/xy/xy.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace geo {
namespace {

bool xy_less(const Xy& a, const Xy& b) {
  if (a.x != b.x) {
    return a.x < b.x;
  }
  return a.y < b.y;
}

double cross(const Xy& o, const Xy& a, const Xy& b) {
  return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

}  // namespace

std::vector<Xy> convex_hull_xy(std::vector<Xy> pts) {
  std::sort(pts.begin(), pts.end(), xy_less);
  pts.erase(std::unique(pts.begin(), pts.end(),
                        [](const Xy& a, const Xy& b) {
                          return a.x == b.x && a.y == b.y;
                        }),
            pts.end());
  if (pts.size() <= 1) {
    return pts;
  }
  std::vector<Xy> lower;
  for (const Xy& p : pts) {
    while (lower.size() >= 2 &&
           cross(lower[lower.size() - 2], lower.back(), p) <= 0.0) {
      lower.pop_back();
    }
    lower.push_back(p);
  }
  std::vector<Xy> upper;
  for (auto it = pts.rbegin(); it != pts.rend(); ++it) {
    while (upper.size() >= 2 &&
           cross(upper[upper.size() - 2], upper.back(), *it) <= 0.0) {
      upper.pop_back();
    }
    upper.push_back(*it);
  }
  lower.pop_back();
  upper.pop_back();
  lower.insert(lower.end(), upper.begin(), upper.end());
  return lower;
}

double polygon_area_xy(const std::vector<Xy>& ring) {
  if (ring.size() < 3) {
    return 0.0;
  }
  double sum = 0.0;
  for (size_t i = 0; i < ring.size(); ++i) {
    const Xy& a = ring[i];
    const Xy& b = ring[(i + 1) % ring.size()];
    sum += a.x * b.y - b.x * a.y;
  }
  return 0.5 * std::fabs(sum);
}

double bbox_area_xy(const std::vector<Xy>& pts) {
  if (pts.empty()) {
    return 0.0;
  }
  double min_x = pts[0].x;
  double min_y = pts[0].y;
  double max_x = pts[0].x;
  double max_y = pts[0].y;
  for (const Xy& p : pts) {
    min_x = std::min(min_x, p.x);
    min_y = std::min(min_y, p.y);
    max_x = std::max(max_x, p.x);
    max_y = std::max(max_y, p.y);
  }
  return std::max(0.0, max_x - min_x) * std::max(0.0, max_y - min_y);
}

}  // namespace geo
