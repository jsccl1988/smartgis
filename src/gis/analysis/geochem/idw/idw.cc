// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/idw/idw.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "gis/analysis/geochem/idw/idw_profile.h"
#include "gis/analysis/geochem/stats/stats.h"

namespace gis {
namespace detail {

GeochemIdwResult run_idw(const GeochemSampleSet& set,
                         std::string_view element,
                         int cell_count,
                         double power,
                         double threshold,
                         double k_sigma) {
  GeochemIdwResult out;
  out.element = std::string(element);
  if (!set.ok || set.samples.empty()) {
    out.error = set.error.empty() ? "bad_samples" : set.error;
    return out;
  }
  const int idx = element_index(set, element);
  if (idx < 0) {
    out.error = "element_missing";
    return out;
  }
  if (cell_count < 8) {
    cell_count = 64;
  }
  if (!(power > 0.0)) {
    power = 2.0;
  }

  struct Pt {
    double x;
    double y;
    double v;
  };
  std::vector<Pt> pts;
  pts.reserve(set.samples.size());
  double min_x = 0;
  double max_x = 0;
  double min_y = 0;
  double max_y = 0;
  bool first = true;
  for (const GeochemSample& s : set.samples) {
    if (static_cast<size_t>(idx) >= s.values.size()) {
      continue;
    }
    const double v = s.values[static_cast<size_t>(idx)];
    if (!std::isfinite(v)) {
      continue;
    }
    pts.push_back({s.x, s.y, v});
    if (first) {
      min_x = max_x = s.x;
      min_y = max_y = s.y;
      first = false;
    } else {
      min_x = (std::min)(min_x, s.x);
      max_x = (std::max)(max_x, s.x);
      min_y = (std::min)(min_y, s.y);
      max_y = (std::max)(max_y, s.y);
    }
  }
  if (pts.size() < 3) {
    out.error = "too_few_samples";
    return out;
  }

  const double dx = (std::max)(1e-9, max_x - min_x);
  const double dy = (std::max)(1e-9, max_y - min_y);
  const double pad_x = 0.05 * dx;
  const double pad_y = 0.05 * dy;
  min_x -= pad_x;
  max_x += pad_x;
  min_y -= pad_y;
  max_y += pad_y;

  const double aspect = (max_x - min_x) / (max_y - min_y);
  int width = cell_count;
  int height = cell_count;
  if (aspect >= 1.0) {
    height = (std::max)(8, static_cast<int>(std::lround(cell_count / aspect)));
  } else {
    width = (std::max)(8, static_cast<int>(std::lround(cell_count * aspect)));
  }

  const double px = (max_x - min_x) / static_cast<double>(width);
  const double py = (max_y - min_y) / static_cast<double>(height);
  // North-up GeoTransform (origin UL).
  out.geotransform[0] = min_x;
  out.geotransform[1] = px;
  out.geotransform[2] = 0;
  out.geotransform[3] = max_y;
  out.geotransform[4] = 0;
  out.geotransform[5] = -py;
  out.width = width;
  out.height = height;
  out.values.assign(static_cast<size_t>(width * height), -9999.0f);

  constexpr int kParallelIdwMinRows = 8;
  constexpr long long kParallelIdwMinPixels = 4096;
  float* values = out.values.data();
  const auto fill_row = [&](int row) {
    for (int col = 0; col < width; ++col) {
      const double cx = min_x + (static_cast<double>(col) + 0.5) * px;
      const double cy = max_y - (static_cast<double>(row) + 0.5) * py;
      double wsum = 0;
      double vsum = 0;
      bool exact = false;
      for (const Pt& p : pts) {
        const double ddx = cx - p.x;
        const double ddy = cy - p.y;
        const double dist2 = ddx * ddx + ddy * ddy;
        if (dist2 < 1e-18) {
          values[static_cast<size_t>(row * width + col)] =
              static_cast<float>(p.v);
          exact = true;
          break;
        }
        const double w = 1.0 / std::pow(std::sqrt(dist2), power);
        wsum += w;
        vsum += w * p.v;
      }
      if (!exact && wsum > 0.0) {
        values[static_cast<size_t>(row * width + col)] =
            static_cast<float>(vsum / wsum);
      }
    }
  };

  const bool use_parallel =
      static_cast<long long>(width) * static_cast<long long>(height) >=
          kParallelIdwMinPixels &&
      height >= kParallelIdwMinRows;
  GeochemIdwProfile prof;
  prof.width = width;
  prof.height = height;
  prof.n_pts = static_cast<int>(pts.size());
  prof.work = static_cast<long long>(width) * static_cast<long long>(height) *
              static_cast<long long>(pts.size());
  prof.backend = use_parallel ? "parallel" : "serial";
  const auto t0 = std::chrono::steady_clock::now();
  if (use_parallel) {
    base::execution::GlobalNThreadPoolExecutor executor;
    const auto t1 = std::chrono::steady_clock::now();
    base::execution::parallel_for(executor, 0, height, fill_row);
    const auto t2 = std::chrono::steady_clock::now();
    prof.dispatch_ms = idw_elapsed_ms(t0, t1);
    prof.compute_ms = idw_elapsed_ms(t1, t2);
  } else {
    const auto t1 = std::chrono::steady_clock::now();
    for (int row = 0; row < height; ++row) {
      fill_row(row);
    }
    const auto t2 = std::chrono::steady_clock::now();
    prof.dispatch_ms = idw_elapsed_ms(t0, t1);
    prof.compute_ms = idw_elapsed_ms(t1, t2);
  }
  emit_idw_profile(prof);

  if (!std::isfinite(threshold)) {
    const GeochemElementStats st =
        compute_stats(set, element, 10, k_sigma > 0 ? k_sigma : 2.0);
    threshold = st.ok ? st.threshold : 0.0;
  }
  out.threshold = threshold;
  out.anomaly_mask.assign(static_cast<size_t>(width * height), 0);
  for (size_t i = 0; i < out.values.size(); ++i) {
    const float v = out.values[i];
    if (std::isfinite(v) && v >= static_cast<float>(threshold)) {
      out.anomaly_mask[i] = 1;
    }
  }
  out.ok = true;
  return out;
}

}  // namespace detail
}  // namespace gis
