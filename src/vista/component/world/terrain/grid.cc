// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/grid.h"

#include <algorithm>
#include <cmath>

#include "vista/component/world/terrain/policy.h"

namespace vista {

namespace {

void emit_nested_tile(double view_minx, double view_miny, double view_maxx,
                      double view_maxy, double minx, double miny, double maxx,
                      double maxy, float camera_distance, int ring,
                      std::vector<NestedGridTile>* out) {
  if (!out || !(maxx > minx) || !(maxy > miny)) {
    return;
  }
  NestedGridTile tile;
  tile.minx = minx;
  tile.miny = miny;
  tile.maxx = maxx;
  tile.maxy = maxy;
  tile.ring = ring;
  const float patch_dist = terrain_lod_patch_distance(
      camera_distance, view_minx, view_miny, view_maxx, view_maxy, minx, miny,
      maxx, maxy);
  tile.max_edge = terrain_lod_max_edge(patch_dist);
  tile.morph_weight = terrain_lod_morph_weight(patch_dist, ring);
  out->push_back(tile);
}

}  // namespace
size_t select_nested_grid_tiles(double minx, double miny, double maxx,
                                double maxy, float camera_distance,
                                std::vector<NestedGridTile>* out) {
  if (!(maxx > minx) || !(maxy > miny)) {
    return 0;
  }
  return select_nested_grid_tiles(minx, miny, maxx, maxy, camera_distance,
                                  0.5 * (minx + maxx), 0.5 * (miny + maxy),
                                  out);
}

size_t append_terrain_edge_skirts(std::vector<float>* xyz,
                                  std::vector<uint32_t>* indices,
                                  std::vector<float>* uvs, float skirt_drop) {
  if (!xyz || !indices || xyz->size() < 9 || (xyz->size() % 3) != 0 ||
      indices->size() < 3 || !(skirt_drop > 0.f)) {
    return 0;
  }
  const size_t nvert = xyz->size() / 3;
  float minx = (*xyz)[0];
  float maxx = minx;
  float minz = (*xyz)[2];
  float maxz = minz;
  for (size_t i = 0; i < nvert; ++i) {
    const float x = (*xyz)[i * 3];
    const float z = (*xyz)[i * 3 + 2];
    minx = (std::min)(minx, x);
    maxx = (std::max)(maxx, x);
    minz = (std::min)(minz, z);
    maxz = (std::max)(maxz, z);
  }
  const float span = (std::max)(maxx - minx, maxz - minz);
  if (!(span > 0.f)) {
    return 0;
  }
  const float eps = (std::max)(1.0e-5f * span, 1.0e-6f);
  const bool write_uv =
      uvs && uvs->size() == nvert * 2u;
  struct EdgeV {
    float t = 0.f;
    uint32_t i = 0;
  };
  const size_t tri_before = indices->size() / 3;
  auto emit_edge = [&](bool by_x, float const_axis) {
    std::vector<EdgeV> edge;
    edge.reserve(nvert);
    for (uint32_t i = 0; i < static_cast<uint32_t>(nvert); ++i) {
      const float x = (*xyz)[static_cast<size_t>(i) * 3];
      const float z = (*xyz)[static_cast<size_t>(i) * 3 + 2];
      const float c = by_x ? x : z;
      if (std::fabs(c - const_axis) > eps) {
        continue;
      }
      EdgeV ev;
      ev.t = by_x ? z : x;
      ev.i = i;
      edge.push_back(ev);
    }
    if (edge.size() < 2) {
      return;
    }
    std::sort(edge.begin(), edge.end(),
              [](const EdgeV& a, const EdgeV& b) { return a.t < b.t; });
    for (size_t k = 1; k < edge.size(); ++k) {
      if (std::fabs(edge[k].t - edge[k - 1].t) <= eps) {
        continue;
      }
      const uint32_t i0 = edge[k - 1].i;
      const uint32_t i1 = edge[k].i;
      const size_t b0 = static_cast<size_t>(i0) * 3u;
      const size_t b1 = static_cast<size_t>(i1) * 3u;
      const uint32_t s0 = static_cast<uint32_t>(xyz->size() / 3);
      xyz->push_back((*xyz)[b0]);
      xyz->push_back((*xyz)[b0 + 1] - skirt_drop);
      xyz->push_back((*xyz)[b0 + 2]);
      xyz->push_back((*xyz)[b1]);
      xyz->push_back((*xyz)[b1 + 1] - skirt_drop);
      xyz->push_back((*xyz)[b1 + 2]);
      if (write_uv) {
        uvs->push_back((*uvs)[static_cast<size_t>(i0) * 2u]);
        uvs->push_back((*uvs)[static_cast<size_t>(i0) * 2u + 1]);
        uvs->push_back((*uvs)[static_cast<size_t>(i1) * 2u]);
        uvs->push_back((*uvs)[static_cast<size_t>(i1) * 2u + 1]);
      }
      const uint32_t s1 = s0 + 1;
      indices->push_back(i0);
      indices->push_back(i1);
      indices->push_back(s1);
      indices->push_back(i0);
      indices->push_back(s1);
      indices->push_back(s0);
    }
  };
  emit_edge(true, minx);
  emit_edge(true, maxx);
  emit_edge(false, minz);
  emit_edge(false, maxz);
  const size_t tri_after = indices->size() / 3;
  return tri_after > tri_before ? tri_after - tri_before : 0;
}

size_t select_nested_grid_tiles(double minx, double miny, double maxx,
                                double maxy, float camera_distance,
                                double focus_x, double focus_y,
                                std::vector<NestedGridTile>* out) {
  if (!out || !(maxx > minx) || !(maxy > miny)) {
    return 0;
  }
  out->clear();
  const int rings = terrain_lod_nested_rings(camera_distance);
  double cx = focus_x;
  double cy = focus_y;
  if (!(cx >= minx && cx <= maxx) || !(cy >= miny && cy <= maxy) ||
      !std::isfinite(cx) || !std::isfinite(cy)) {
    cx = (std::min)(maxx, (std::max)(minx, cx));
    cy = (std::min)(maxy, (std::max)(miny, cy));
    if (!std::isfinite(cx) || !std::isfinite(cy)) {
      cx = 0.5 * (minx + maxx);
      cy = 0.5 * (miny + maxy);
    }
  }
  const double hx = (std::max)(cx - minx, maxx - cx);
  const double hy = (std::max)(cy - miny, maxy - cy);
  for (int r = 0; r < rings; ++r) {
    const int outer_shift = rings - 1 - r;
    const double scale =
        1.0 / static_cast<double>(1 << outer_shift);
    const double ohx = hx * scale;
    const double ohy = hy * scale;
    const double ominx = cx - ohx;
    const double omaxx = cx + ohx;
    const double ominy = cy - ohy;
    const double omaxy = cy + ohy;
    if (r == 0) {
      emit_nested_tile(minx, miny, maxx, maxy, ominx, ominy, omaxx, omaxy,
                       camera_distance, r, out);
      continue;
    }
    const double scale_in =
        1.0 / static_cast<double>(1 << (rings - r));
    const double ihx = hx * scale_in;
    const double ihy = hy * scale_in;
    const double iminx = cx - ihx;
    const double imaxx = cx + ihx;
    const double iminy = cy - ihy;
    const double imaxy = cy + ihy;
    emit_nested_tile(minx, miny, maxx, maxy, ominx, ominy, omaxx, iminy,
                     camera_distance, r, out);
    emit_nested_tile(minx, miny, maxx, maxy, ominx, imaxy, omaxx, omaxy,
                     camera_distance, r, out);
    emit_nested_tile(minx, miny, maxx, maxy, ominx, iminy, iminx, imaxy,
                     camera_distance, r, out);
    emit_nested_tile(minx, miny, maxx, maxy, imaxx, iminy, omaxx, imaxy,
                     camera_distance, r, out);
  }
  return out->size();
}

}  // namespace vista
