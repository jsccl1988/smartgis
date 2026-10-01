// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_GDI_OGR_XY_H_
#define SMT_LEGACY_RENDER_GDI_OGR_XY_H_

#include <vector>

#include "ogrsf_frmts.h"

namespace render {
namespace detail {

// Pack OGR curve verts into float xy[2*n_out].
// step==1: one getPoints(OGRRawPoint*) bulk read (avoids per-vert getX/getY).
// step>1: strided getX/getY only — full getPoints would copy verts we drop.
// When |keep_last| and step>1, appends the final vertex if the stride drops it.
inline int pack_curve_xy_strided(const OGRLineString* curve, int step,
                                 std::vector<float>* xy, bool keep_last) {
  if (!curve || !xy) {
    return 0;
  }
  const int n = curve->getNumPoints();
  if (n < 1) {
    return 0;
  }
  const int stride = step < 1 ? 1 : step;
  const int sampled = (n + stride - 1) / stride;
  const int reserve =
      sampled + ((keep_last && stride > 1 && ((n - 1) % stride) != 0) ? 1 : 0);
  xy->resize(static_cast<size_t>(reserve) * 2u);

  if (stride == 1) {
    thread_local std::vector<OGRRawPoint> raw;
    raw.resize(static_cast<size_t>(n));
    curve->getPoints(raw.data());
    for (int i = 0; i < n; ++i) {
      const OGRRawPoint& p = raw[static_cast<size_t>(i)];
      (*xy)[static_cast<size_t>(i) * 2u] = static_cast<float>(p.x);
      (*xy)[static_cast<size_t>(i) * 2u + 1u] = static_cast<float>(p.y);
    }
    return n;
  }

  int n_out = 0;
  for (int i = 0; i < n; i += stride, ++n_out) {
    (*xy)[static_cast<size_t>(n_out) * 2u] =
        static_cast<float>(curve->getX(i));
    (*xy)[static_cast<size_t>(n_out) * 2u + 1u] =
        static_cast<float>(curve->getY(i));
  }
  if (keep_last && ((n - 1) % stride) != 0) {
    (*xy)[static_cast<size_t>(n_out) * 2u] =
        static_cast<float>(curve->getX(n - 1));
    (*xy)[static_cast<size_t>(n_out) * 2u + 1u] =
        static_cast<float>(curve->getY(n - 1));
    ++n_out;
  }
  return n_out;
}

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_GDI_OGR_XY_H_
