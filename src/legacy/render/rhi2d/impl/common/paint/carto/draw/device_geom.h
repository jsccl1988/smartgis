// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_GDI_DEVICE_GEOM_H_
#define SMT_LEGACY_RENDER_GDI_DEVICE_GEOM_H_

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <vector>

#include "base/math/affine2.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/frame/context.h"

namespace render {
namespace detail {

inline LpToDp2 make_lp_to_dp(const SmtRenderContext& rc) {
  LpToDp2 a;
  a.wox = rc.windowport.m_fWOX;
  a.woy = rc.windowport.m_fWOY;
  a.vox = rc.viewport.m_fVOX;
  a.voy = rc.viewport.m_fVOY;
  a.scale = rc.fblc;
  a.view_h = rc.viewport.m_fVHeight;
  a.flip_y = true;
  return a;
}

// Overview / mid-scale: drop denser device samples (Chebyshev cell size).
// China showcase fblc sits ~8–12 — cell>=2 must cover that band.
// Zoomed-in (large fblc) keeps exact-pixel dedupe only (return 1).
inline int overview_thin_chebyshev(float scale) {
  if (scale < 6.f) {
    return 4;
  }
  if (scale < 12.f) {
    return 3;
  }
  if (scale < 18.f) {
    return 2;
  }
  return 1;
}

// Vertex stride before LP→DP. Tuned so china overview (~fblc 10) still
// subsamples dense coasts/roads before transform.
inline int overview_vertex_step(int n, float scale) {
  int step = 1;
  if (n > 20 && scale < 24.f) {
    step = (std::max)(1, n / 48);
  }
  if (n > 32 && scale < 16.f) {
    step = (std::max)(step, n / 56);
  }
  if (n > 48 && scale < 12.f) {
    step = (std::max)(step, n / 36);
  }
  if (n > 96 && scale < 10.f) {
    step = (std::max)(step, n / 28);
  }
  if (n > 192 && scale < 7.f) {
    step = (std::max)(step, n / 20);
  }
  return step;
}

inline int chebyshev_dist(long x0, long y0, long x1, long y1) {
  const long dx = std::labs(x1 - x0);
  const long dy = std::labs(y1 - y0);
  return static_cast<int>((std::max)(dx, dy));
}

// Collapse device samples closer than |min_cell| (Chebyshev). Keeps rings
// closed and >= 3. |min_cell| <= 1 matches exact-pixel dedupe.
inline int thin_device_ring(const POINT* in, int n, std::vector<POINT>* out,
                            int min_cell = 1) {
  if (!in || !out || n < 2) {
    return 0;
  }
  const int cell = (std::max)(1, min_cell);
  const size_t base = out->size();
  long last_x = LONG_MIN;
  long last_y = LONG_MIN;
  for (int i = 0; i < n; ++i) {
    const POINT& p = in[i];
    if (i == 0 || chebyshev_dist(last_x, last_y, p.x, p.y) >= cell) {
      out->push_back(p);
      last_x = p.x;
      last_y = p.y;
    }
  }
  int kept = static_cast<int>(out->size() - base);
  if (kept >= 2) {
    const POINT& first = (*out)[base];
    const POINT& last = out->back();
    if (first.x != last.x || first.y != last.y) {
      out->push_back(first);
      ++kept;
    }
  }
  if (kept < 3) {
    out->resize(base);
    for (int i = 0; i < n; ++i) {
      out->push_back(in[i]);
    }
    kept = n;
  }
  return kept;
}

// Open polylines: drop samples inside |min_cell|; always keep endpoints.
inline int thin_device_polyline(const POINT* in, int n, std::vector<POINT>* out,
                                int min_cell = 1) {
  if (!in || !out || n < 2) {
    return 0;
  }
  const int cell = (std::max)(1, min_cell);
  const size_t base = out->size();
  out->push_back(in[0]);
  long last_x = in[0].x;
  long last_y = in[0].y;
  for (int i = 1; i < n - 1; ++i) {
    const POINT& p = in[i];
    if (chebyshev_dist(last_x, last_y, p.x, p.y) >= cell) {
      out->push_back(p);
      last_x = p.x;
      last_y = p.y;
    }
  }
  const POINT& end = in[n - 1];
  if (end.x != out->back().x || end.y != out->back().y) {
    if (chebyshev_dist(out->back().x, out->back().y, end.x, end.y) < cell &&
        static_cast<int>(out->size() - base) >= 2) {
      out->back() = end;
    } else {
      out->push_back(end);
    }
  }
  int kept = static_cast<int>(out->size() - base);
  if (kept < 2) {
    out->resize(base);
    for (int i = 0; i < n; ++i) {
      out->push_back(in[i]);
    }
    kept = n;
  }
  return kept;
}

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_GDI_DEVICE_GEOM_H_
