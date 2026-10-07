// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/host/scenic_scene_bind.h"

#include "content/browser/camera/gis_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"

#include <algorithm>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
namespace detail {
namespace {

scenic::GeomKind to_scenic_kind(GisScene::GeomKind kind) {
  switch (kind) {
    case GisScene::GeomKind::kLine:
      return scenic::GeomKind::kLine;
    case GisScene::GeomKind::kPolygon:
      return scenic::GeomKind::kPolygon;
    case GisScene::GeomKind::kText:
      return scenic::GeomKind::kText;
    case GisScene::GeomKind::kPoint:
      return scenic::GeomKind::kPoint;
  }
  return scenic::GeomKind::kPoint;
}

// Draft item stores a vertex offset until |xy| is finalized (avoids holding
// raw pointers into a vector that may reallocate while filling).
struct DraftItem {
  scenic::DrawItem item;
  size_t vert_begin = 0;
};

}  // namespace

void fill_scenic_draw_items(const GisScene* scene, double scale,
                            std::vector<scenic::Vertex2>* xy,
                            std::vector<scenic::DrawItem>* items) {
  if (!xy || !items) {
    return;
  }
  xy->clear();
  items->clear();
  if (!scene) {
    return;
  }

  size_t vert_n = 0;
  size_t feat_n = 0;
  for (const GisScene::Layer& layer : scene->layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const GisScene::Feature& f : layer.features) {
      if (f.points.empty()) {
        continue;
      }
      ++feat_n;
      vert_n += f.points.size();
    }
  }
  // Harness/china documents can be huge; Scenic GDI is a present proof path.
  // Never prefix-truncate rings: GDI Polygon closes last→first, and a prefix
  // cut draws a long chord (diagonal "clip") across China.
  // Prefer large polygons first so the national outline / provinces fill the
  // frame before the feature budget is spent on city points.
  constexpr size_t kMaxPolygons = 2500;
  constexpr size_t kMaxLines = 800;
  constexpr size_t kMaxPoints = 700;
  constexpr size_t kMaxFeatures = kMaxPolygons + kMaxLines + kMaxPoints;
  constexpr size_t kMaxVertsPerFeature = 2500;
  const size_t vert_cap =
      std::min(vert_n, kMaxFeatures * kMaxVertsPerFeature);

  struct RankedFeature {
    const GisScene::Layer* layer = nullptr;
    const GisScene::Feature* feature = nullptr;
    scenic::GeomKind kind = scenic::GeomKind::kPoint;
    size_t vertex_count = 0;
  };
  std::vector<RankedFeature> ranked;
  ranked.reserve(feat_n);
  for (const GisScene::Layer& layer : scene->layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const GisScene::Feature& f : layer.features) {
      if (f.points.empty()) {
        continue;
      }
      RankedFeature r;
      r.layer = &layer;
      r.feature = &f;
      r.kind = to_scenic_kind(f.kind);
      r.vertex_count = f.points.size();
      ranked.push_back(r);
    }
  }
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const RankedFeature& a, const RankedFeature& b) {
                     const int ka = (a.kind == scenic::GeomKind::kPolygon) ? 0
                                   : (a.kind == scenic::GeomKind::kLine)   ? 1
                                                                          : 2;
                     const int kb = (b.kind == scenic::GeomKind::kPolygon) ? 0
                                   : (b.kind == scenic::GeomKind::kLine)   ? 1
                                                                          : 2;
                     if (ka != kb) {
                       return ka < kb;
                     }
                     return a.vertex_count > b.vertex_count;
                   });

  std::vector<scenic::Vertex2> verts;
  std::vector<DraftItem> drafts;
  verts.reserve(vert_cap);
  drafts.reserve(std::min(feat_n, kMaxFeatures));
  size_t n_poly = 0;
  size_t n_line = 0;
  size_t n_point = 0;

  for (const RankedFeature& r : ranked) {
    if (!r.layer || !r.feature) {
      continue;
    }
    if (r.kind == scenic::GeomKind::kPolygon) {
      if (n_poly >= kMaxPolygons) {
        continue;
      }
    } else if (r.kind == scenic::GeomKind::kLine) {
      if (n_line >= kMaxLines) {
        continue;
      }
    } else if (n_point >= kMaxPoints) {
      continue;
    }

    const GisScene::Feature& f = *r.feature;
    COLORREF fill = RGB(196, 214, 160);
    COLORREF stroke = RGB(40, 50, 60);
    int stroke_w = 1;
    (void)scene->style_colors_for_feature(*r.layer, f, scale, &fill, &stroke,
                                          &stroke_w);
    DraftItem draft;
    draft.vert_begin = verts.size();
    draft.item.kind = r.kind;
    draft.item.fill_colorref = static_cast<uint32_t>(fill);
    draft.item.stroke_colorref = static_cast<uint32_t>(stroke);
    draft.item.stroke_width_px = stroke_w;
    const size_t src_n = f.points.size();
    const size_t step =
        src_n > kMaxVertsPerFeature
            ? (src_n + kMaxVertsPerFeature - 1) / kMaxVertsPerFeature
            : 1u;
    size_t written = 0;
    for (size_t vi = 0; vi < src_n; vi += step) {
      const GisScene::Vertex& p = f.points[vi];
      verts.push_back(scenic::Vertex2{static_cast<float>(p.x),
                                      static_cast<float>(p.y)});
      ++written;
    }
    // Keep ring closure for polygons/lines when the last sample was skipped.
    if (src_n > 1 && step > 1 && ((src_n - 1) % step) != 0) {
      const GisScene::Vertex& p = f.points[src_n - 1];
      verts.push_back(scenic::Vertex2{static_cast<float>(p.x),
                                      static_cast<float>(p.y)});
      ++written;
    }
    draft.item.vertex_count = static_cast<uint32_t>(written);
    drafts.push_back(draft);
    if (r.kind == scenic::GeomKind::kPolygon) {
      ++n_poly;
    } else if (r.kind == scenic::GeomKind::kLine) {
      ++n_line;
    } else {
      ++n_point;
    }
  }

  *xy = std::move(verts);
  items->clear();
  items->reserve(drafts.size());
  for (DraftItem& draft : drafts) {
    draft.item.xy = xy->data() + draft.vert_begin;
    items->push_back(draft.item);
  }
}

scenic::ViewXform scenic_view_from_frame(const ViewFrame* frame) {
  scenic::ViewXform view;
  if (!frame) {
    return view;
  }
  view.pan_x = frame->pan_x();
  view.pan_y = frame->pan_y();
  view.scale = frame->scale();
  return view;
}

scenic::OrbitXform scenic_orbit_from_host(const OrbitFrame* orbit,
                                          const GisScene* scene) {
  scenic::OrbitXform xform;
  const Extent2 box = orbit ? orbit->world_extent() : kChinaLonLatExtent;
  const Extent2 e = china_or(box);
  xform.lon_min = e.xmin;
  xform.lat_min = e.ymin;
  xform.lon_max = e.xmax;
  xform.lat_max = e.ymax;
  if (orbit) {
    xform.yaw = orbit->yaw();
    xform.pitch = orbit->pitch();
    xform.distance = orbit->distance();
  }
  if (scene && scene->has_china_extent()) {
    double min_x = 0;
    double min_y = 0;
    double max_x = 0;
    double max_y = 0;
    if (scene->compute_extent(&min_x, &min_y, &max_x, &max_y)) {
      // Map Y is -lat; orbit envelope is lon/lat.
      xform.lon_min = min_x;
      xform.lon_max = max_x;
      xform.lat_min = -max_y;
      xform.lat_max = -min_y;
    }
  }
  return xform;
}

}  // namespace detail
}  // namespace content
