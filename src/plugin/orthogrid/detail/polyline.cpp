// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/orthogrid/detail/polyline.h"

#include "base/core/core.h"

#include <cmath>

namespace orthogrid {
namespace detail {
namespace {

double vec2_len(double x, double y) {
  return std::sqrt(x * x + y * y);
}

double cos_abc(const base::dbfPoint& a,
               const base::dbfPoint& b,
               const base::dbfPoint& c) {
  const double bax = a.x - b.x;
  const double bay = a.y - b.y;
  const double bcx = c.x - b.x;
  const double bcy = c.y - b.y;
  const double denom = vec2_len(bcx, bcy) * vec2_len(bax, bay);
  if (denom == 0.0) {
    return 1.0;
  }
  return (bax * bcx + bay * bcy) / denom;
}

}  // namespace

double distance_to_polyline(const base::dbfPoint& pt,
                            std::vector<base::dbfPoint>& points,
                            int& index_pre,
                            int& index_next) {
  if (points.size() < 2) {
    return SMT_C_INVALID_DBF_VALUE;
  }

  base::dbfPoint pt_cur(pt.x, pt.y);
  float min_theta = 1.0f;
  for (int i = 0; i < static_cast<int>(points.size()) - 1; ++i) {
    const float theta =
        static_cast<float>(cos_abc(points[i], pt_cur, points[i + 1]));
    if (theta < min_theta) {
      index_pre = i;
      index_next = i + 1;
      min_theta = theta;
    }
  }

  const base::dbfPoint& last = points[points.size() - 1];
  const base::dbfPoint& first = points[0];
  if (last.x == first.x && last.y == first.y) {
    const float theta = static_cast<float>(cos_abc(last, pt_cur, first));
    if (theta < min_theta) {
      index_pre = static_cast<int>(points.size()) - 1;
      index_next = 0;
      min_theta = theta;
    }
  }

  const base::dbfPoint& pre = points[index_pre];
  const base::dbfPoint& next = points[index_next];
  const double theta = cos_abc(pre, next, pt_cur);
  const float sin_abc = static_cast<float>(std::sqrt(1.0 - theta * theta));
  return std::fabs(sin_abc * vec2_len(pt_cur.x - next.x, pt_cur.y - next.y));
}

long locate_on_polyline(base::dbfPoint& pt,
                        std::vector<base::dbfPoint>& points,
                        int& index_pre,
                        int& index_next) {
  if (points.size() < 2) {
    return SMT_ERR_INVALID_PARAM;
  }

  float min_theta = 1.0f;
  const int start = index_pre;
  for (int i = start; i < static_cast<int>(points.size()) - 1; ++i) {
    const float temp =
        static_cast<float>(cos_abc(points[i], pt, points[i + 1]));
    if (temp < min_theta) {
      index_pre = i;
      index_next = i + 1;
      min_theta = temp;
    }
    if (min_theta == -1.0f) {
      return SMT_ERR_NONE;
    }
  }
  return SMT_ERR_FAILURE;
}

int foot_on_segment(base::dbfPoint a,
                    base::dbfPoint b,
                    base::dbfPoint p,
                    base::dbfPoint& h) {
  int flag = 0;
  if (a.x == b.x && a.y == b.y) {
    return 2;
  }
  if (a.x == b.x) {
    h.x = a.x;
    h.y = p.y;
    const float m = static_cast<float>(p.y - a.y);
    const float n = static_cast<float>(p.y - b.y);
    if (m * n < 0) {
      flag = 0;
    } else if (std::fabs(m) < std::fabs(n)) {
      flag = -1;
    } else {
      flag = 1;
    }
    return flag;
  }
  if (a.y == b.y) {
    h.y = a.y;
    h.x = p.x;
    const float m = static_cast<float>(p.x - a.x);
    const float n = static_cast<float>(p.x - b.x);
    if (m * n < 0) {
      flag = 0;
    } else if (std::fabs(m) < std::fabs(n)) {
      flag = -1;
    } else {
      flag = 1;
    }
    return flag;
  }

  const float k = static_cast<float>((b.y - a.y) / (b.x - a.x));
  h.x = (k * k * a.x + k * (p.y - a.y) + p.x) / (k * k + 1);
  h.y = k * (h.x - a.x) + a.y;
  const float m = static_cast<float>(h.x - a.x);
  const float n = static_cast<float>(h.x - b.x);
  if (m * n < 0) {
    return 0;
  }
  if (std::fabs(m) < std::fabs(n)) {
    return -1;
  }
  return 1;
}

}  // namespace detail
}  // namespace orthogrid
