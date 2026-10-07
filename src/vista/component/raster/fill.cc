// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/raster/fill.h"

#include <algorithm>
#include <cstdint>

namespace vista {
namespace raster {
namespace {

#if defined(_MSC_VER)
#define VISTA_RASTER_FILL_INLINE __forceinline
#else
#define VISTA_RASTER_FILL_INLINE inline
#endif

// Coverage blend into an existing BGRA pixel (A stays opaque).
// |cov_256| is 1..256 (256 = opaque). Integer path avoids float per edge px.
VISTA_RASTER_FILL_INLINE void blend_bgra_coverage_u8(uint32_t* dst,
                                                     uint32_t src_bgra,
                                                     int cov_256) {
  if (!dst || cov_256 <= 0) {
    return;
  }
  if (cov_256 >= 256) {
    *dst = src_bgra | 0xff000000u;
    return;
  }
  const int ic = 256 - cov_256;
  const uint32_t d = *dst;
  const int db = static_cast<int>(d & 0xff);
  const int dg = static_cast<int>((d >> 8) & 0xff);
  const int dr = static_cast<int>((d >> 16) & 0xff);
  const int sb = static_cast<int>(src_bgra & 0xff);
  const int sg = static_cast<int>((src_bgra >> 8) & 0xff);
  const int sr = static_cast<int>((src_bgra >> 16) & 0xff);
  const int ob = (db * ic + sb * cov_256 + 128) >> 8;
  const int og = (dg * ic + sg * cov_256 + 128) >> 8;
  const int o_r = (dr * ic + sr * cov_256 + 128) >> 8;
  *dst = 0xff000000u | (static_cast<uint32_t>(o_r) << 16) |
         (static_cast<uint32_t>(og) << 8) | static_cast<uint32_t>(ob);
}

// 16.16 fixed edge: x at integer scanline y, stepped by dx_dy each row.
struct FixedEdge {
  int64_t x_fp = 0;  // current x << 16
  int64_t dx_fp = 0;
};

VISTA_RASTER_FILL_INLINE FixedEdge make_edge(POINT p0, POINT p1) {
  FixedEdge e{};
  // Sample at pixel center of the first row that the edge covers.
  const int y0 = static_cast<int>(p0.y);
  const int y1 = static_cast<int>(p1.y);
  if (y1 == y0) {
    e.x_fp = (static_cast<int64_t>(p0.x) << 16) + 0x8000;
    e.dx_fp = 0;
    return e;
  }
  const int64_t dy = static_cast<int64_t>(y1) - static_cast<int64_t>(y0);
  e.dx_fp = ((static_cast<int64_t>(p1.x) - static_cast<int64_t>(p0.x)) << 16) /
            dy;
  // x(y0 + 0.5) in 16.16 — matches prior double lerp at pixel centers.
  e.x_fp = (static_cast<int64_t>(p0.x) << 16) + 0x8000 + (e.dx_fp >> 1);
  return e;
}

}  // namespace

void fill_tri_solid(DibSurface* dib, POINT a, POINT b, POINT c, uint32_t bgra) {
  if (!dib || !dib->valid()) {
    return;
  }
  // Sort by y then x for stable spans.
  if (b.y < a.y || (b.y == a.y && b.x < a.x)) {
    const POINT t = a;
    a = b;
    b = t;
  }
  if (c.y < a.y || (c.y == a.y && c.x < a.x)) {
    const POINT t = a;
    a = c;
    c = t;
  }
  if (c.y < b.y || (c.y == b.y && c.x < b.x)) {
    const POINT t = b;
    b = c;
    c = t;
  }
  if (c.y == a.y) {
    return;  // zero-height
  }

  const int min_y = (std::max)(0, static_cast<int>(a.y));
  const int max_y = (std::min)(dib->height - 1, static_cast<int>(c.y));
  if (min_y > max_y) {
    return;
  }

  const int ay = static_cast<int>(a.y);
  const int by = static_cast<int>(b.y);
  const bool flat_top = (by == ay);

  // Long edge a→c always active. Short edge is a→b then b→c (or b→c when
  // flat-top).
  FixedEdge edge_long = make_edge(a, c);
  FixedEdge edge_short = flat_top ? make_edge(b, c) : make_edge(a, b);
  FixedEdge edge_bc = make_edge(b, c);

  // Advance edges from a.y to the first visible scanline.
  if (min_y > ay) {
    const int skip = min_y - ay;
    edge_long.x_fp += edge_long.dx_fp * skip;
    if (!flat_top) {
      if (min_y < by) {
        edge_short.x_fp += edge_short.dx_fp * skip;
      } else {
        // Landed past the knee — switch to b→c and skip from b.y.
        edge_short = edge_bc;
        if (min_y > by) {
          edge_short.x_fp += edge_short.dx_fp * (min_y - by);
        }
      }
    } else {
      edge_short.x_fp += edge_short.dx_fp * skip;
    }
  }

  for (int y = min_y; y <= max_y; ++y) {
    if (!flat_top && y == by && y > ay) {
      // Switch short edge at the knee (integer row == b.y).
      edge_short = edge_bc;
    }
    int64_t x0_fp = edge_long.x_fp;
    int64_t x1_fp = edge_short.x_fp;
    if (x1_fp < x0_fp) {
      const int64_t tmp = x0_fp;
      x0_fp = x1_fp;
      x1_fp = tmp;
    }
    // Subpixel coverage from 16.16 fractions (low 16 bits).
    const int x_left = static_cast<int>(x0_fp >> 16);
    const int x_right = static_cast<int>(x1_fp >> 16);
    uint32_t* row = dib->row(y);
    if (x_left == x_right) {
      if (x_left >= 0 && x_left < dib->width) {
        const int cov =
            static_cast<int>(((x1_fp - x0_fp) + 128) >> 8);  // → 0..256 scale
        blend_bgra_coverage_u8(row + x_left, bgra, (std::min)(256, cov));
      }
    } else {
      if (x_left >= 0 && x_left < dib->width) {
        // Left coverage: 1 - frac(x0)
        const int cov = 256 - static_cast<int>((x0_fp & 0xffff) >> 8);
        blend_bgra_coverage_u8(row + x_left, bgra, cov);
      }
      const int xa = (std::max)(0, x_left + 1);
      const int xb = (std::min)(dib->width - 1, x_right - 1);
      if (xa <= xb) {
        std::fill(row + xa, row + xb + 1, bgra | 0xff000000u);
      }
      if (x_right >= 0 && x_right < dib->width) {
        const int cov = static_cast<int>((x1_fp & 0xffff) >> 8);
        blend_bgra_coverage_u8(row + x_right, bgra, (std::min)(256, cov));
      }
    }
    edge_long.x_fp += edge_long.dx_fp;
    edge_short.x_fp += edge_short.dx_fp;
  }
}

}  // namespace raster
}  // namespace vista
