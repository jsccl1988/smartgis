// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/contour/smooth.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vista {
namespace detail {
namespace {

struct ContourPt2 {
  float x = 0.f;
  float y = 0.f;
};

uint64_t quantize_contour_xy(float x, float y) {
  const auto qx =
      static_cast<int32_t>(std::llround(static_cast<double>(x) * 1000.0));
  const auto qy =
      static_cast<int32_t>(std::llround(static_cast<double>(y) * 1000.0));
  return (static_cast<uint64_t>(static_cast<uint32_t>(qx)) << 32) |
         static_cast<uint32_t>(qy);
}

void chaikin_open_polyline(std::vector<ContourPt2>* poly) {
  if (!poly || poly->size() < 3u) {
    return;
  }
  std::vector<ContourPt2> next;
  next.reserve(poly->size() * 2u);
  next.push_back(poly->front());
  for (std::size_t i = 0; i + 1u < poly->size(); ++i) {
    const ContourPt2& a = (*poly)[i];
    const ContourPt2& b = (*poly)[i + 1u];
    next.push_back({0.75f * a.x + 0.25f * b.x, 0.75f * a.y + 0.25f * b.y});
    next.push_back({0.25f * a.x + 0.75f * b.x, 0.25f * a.y + 0.75f * b.y});
  }
  next.push_back(poly->back());
  *poly = std::move(next);
}

void chaikin_closed_polyline(std::vector<ContourPt2>* poly) {
  if (!poly || poly->size() < 3u) {
    return;
  }
  const std::size_t n = poly->size();
  std::vector<ContourPt2> next;
  next.reserve(n * 2u);
  for (std::size_t i = 0; i < n; ++i) {
    const ContourPt2& a = (*poly)[i];
    const ContourPt2& b = (*poly)[(i + 1u) % n];
    next.push_back({0.75f * a.x + 0.25f * b.x, 0.75f * a.y + 0.25f * b.y});
    next.push_back({0.25f * a.x + 0.75f * b.x, 0.25f * a.y + 0.75f * b.y});
  }
  *poly = std::move(next);
}

}  // namespace

void smooth_isoline_xy_segments(std::vector<float>* segs, int chaikin_iters) {
  if (!segs || segs->size() < 4u || chaikin_iters <= 0) {
    return;
  }
  const std::size_t nseg = segs->size() / 4u;
  struct EndRef {
    std::size_t seg = 0;
    bool at_start = true;
  };
  std::unordered_map<uint64_t, std::vector<EndRef>> adj;
  adj.reserve(nseg * 2u);
  for (std::size_t i = 0; i < nseg; ++i) {
    const float* s = segs->data() + i * 4u;
    adj[quantize_contour_xy(s[0], s[1])].push_back({i, true});
    adj[quantize_contour_xy(s[2], s[3])].push_back({i, false});
  }

  auto endpoint = [&](std::size_t seg, bool at_start) -> ContourPt2 {
    const float* s = segs->data() + seg * 4u;
    return at_start ? ContourPt2{s[0], s[1]} : ContourPt2{s[2], s[3]};
  };

  std::vector<char> used(nseg, 0);
  std::vector<std::vector<ContourPt2>> polylines;
  polylines.reserve(nseg / 4u + 1u);

  auto walk_from = [&](std::size_t seg0, bool at_start0) {
    if (used[seg0]) {
      return;
    }
    std::vector<ContourPt2> poly;
    poly.reserve(64u);
    std::size_t seg = seg0;
    bool at_start = at_start0;
    poly.push_back(endpoint(seg, at_start));
    while (!used[seg]) {
      used[seg] = 1;
      const ContourPt2 nxt = endpoint(seg, !at_start);
      poly.push_back(nxt);
      const auto it = adj.find(quantize_contour_xy(nxt.x, nxt.y));
      if (it == adj.end()) {
        break;
      }
      std::size_t next_seg = nseg;
      bool next_at_start = false;
      for (const EndRef& er : it->second) {
        if (!used[er.seg]) {
          next_seg = er.seg;
          next_at_start = er.at_start;
          break;
        }
      }
      if (next_seg == nseg) {
        break;
      }
      seg = next_seg;
      at_start = next_at_start;
    }
    if (poly.size() >= 2u) {
      polylines.push_back(std::move(poly));
    }
  };

  for (const auto& kv : adj) {
    if (kv.second.size() != 1u) {
      continue;
    }
    const EndRef& er = kv.second.front();
    walk_from(er.seg, er.at_start);
  }
  for (std::size_t i = 0; i < nseg; ++i) {
    if (!used[i]) {
      walk_from(i, true);
    }
  }

  segs->clear();
  segs->reserve(nseg * 4u *
                (std::size_t{1} << static_cast<unsigned>(chaikin_iters)));
  for (std::vector<ContourPt2>& poly : polylines) {
    bool closed = false;
    if (poly.size() >= 3u &&
        quantize_contour_xy(poly.front().x, poly.front().y) ==
            quantize_contour_xy(poly.back().x, poly.back().y)) {
      closed = true;
      poly.pop_back();
    }
    if (poly.size() < 2u) {
      continue;
    }
    if (closed && poly.size() >= 3u) {
      for (int pass = 0; pass < chaikin_iters; ++pass) {
        chaikin_closed_polyline(&poly);
      }
      for (std::size_t i = 0; i < poly.size(); ++i) {
        const ContourPt2& a = poly[i];
        const ContourPt2& b = poly[(i + 1u) % poly.size()];
        segs->push_back(a.x);
        segs->push_back(a.y);
        segs->push_back(b.x);
        segs->push_back(b.y);
      }
    } else {
      for (int pass = 0; pass < chaikin_iters; ++pass) {
        chaikin_open_polyline(&poly);
      }
      for (std::size_t i = 0; i + 1u < poly.size(); ++i) {
        segs->push_back(poly[i].x);
        segs->push_back(poly[i].y);
        segs->push_back(poly[i + 1u].x);
        segs->push_back(poly[i + 1u].y);
      }
    }
  }
}

}  // namespace detail
}  // namespace vista
