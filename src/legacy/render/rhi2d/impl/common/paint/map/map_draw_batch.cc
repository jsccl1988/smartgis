// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/paint/map/map_draw_batch.h"

#include <chrono>
#include <cstring>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "legacy/render/rhi2d/impl/common/paint/map/map_geom_trace.h"

namespace render {
namespace detail {

void draw_prepared_batch(Rhi2dCartoDraw* carto_draw, int op,
                         std::vector<OGRFeature*>* feats,
                         std::vector<PreparedFeature>* prepared,
                         const FeatureFallbackFn& fallback, DrawBatchMode mode) {
  if (!carto_draw || !feats || !prepared) {
    return;
  }

  // Prep already projected LP?DP into PrepPart::pts (device POINT). Play must
  // only emit draw_device_* — never call lp_to_dp / transform_xy again here.

  // Ordinary lines (no road dual-pen, no river line label) and unlabeled
  // roads (same road_class) can share one PolyPolyline under one style prep.
  auto can_batch_line = [](const PreparedFeature& p) {
    return p.kind == PrepKind::Line && p.road_class == 0 &&
           !(p.is_river && p.anno[0]);
  };
  auto can_batch_road = [](const PreparedFeature& p) {
    return p.kind == PrepKind::Line && p.road_class > 0 && !p.anno[0];
  };
  auto same_line_pen = [](const PreparedFeature& a, const PreparedFeature& b) {
    return a.is_river == b.is_river && a.road_class == b.road_class &&
           a.style.get_style_type() == b.style.get_style_type();
  };
  auto same_road_pen = [](const PreparedFeature& a, const PreparedFeature& b) {
    return a.road_class == b.road_class;
  };

  thread_local std::vector<POINT> batch_pts;
  thread_local std::vector<int> batch_counts;

  auto collect_line_batch = [&](size_t begin, size_t end) {
    batch_pts.clear();
    batch_counts.clear();
    for (size_t k = begin; k < end; ++k) {
      for (const PrepPart& part : (*prepared)[k].parts) {
        size_t offset = 0;
        for (int c : part.counts) {
          if (c >= 2) {
            batch_pts.insert(batch_pts.end(), part.pts.begin() + offset,
                             part.pts.begin() + offset + c);
            batch_counts.push_back(c);
          }
          offset += static_cast<size_t>(c);
        }
      }
    }
  };

  auto play_line_batch = [&](PreparedFeature& prep, size_t begin, size_t end,
                             bool road_batch) {
    carto_draw->feature_type() = prep.feature_type;
    carto_draw->label_priority() = prep.label_priority;
    carto_draw->is_river() = prep.is_river;
    carto_draw->road_class() = prep.road_class;
    carto_draw->anno_angle() = prep.anno_angle;
    carto_draw->anno_buf()[0] = '\0';

    // Roads set dual pens inside draw_device_polylines â€?skip style prep.
    const bool need_style = !road_batch && !carto_draw->lock_style();
    if (need_style) {
      carto_draw->prepare_for_drawing(&prep.style, op);
    }

    collect_line_batch(begin, end);

    const auto draw_begin = base::trace::Trace::time_point::clock::now();
    if (!batch_counts.empty()) {
      carto_draw->draw_device_polylines(batch_pts.data(), batch_counts.data(),
                                    static_cast<int>(batch_counts.size()));
    }
    if (g_active_geom) {
      g_active_geom->line +=
          std::chrono::duration_cast<std::chrono::microseconds>(
              base::trace::Trace::time_point::clock::now() - draw_begin)
              .count();
    }

    if (need_style) {
      carto_draw->end_drawing();
    }
  };

  for (size_t i = 0; i < feats->size();) {
    PreparedFeature& prep = (*prepared)[i];
    OGRFeature* feat = (*feats)[i];
    if (prep.kind == PrepKind::Skip) {
      OGRFeature::DestroyFeature(feat);
      (*feats)[i] = nullptr;
      ++i;
      continue;
    }
    if (prep.kind == PrepKind::Fallback) {
      if (mode == DrawBatchMode::All) {
        if (fallback) {
          fallback(feat, op);
        }
        OGRFeature::DestroyFeature(feat);
        (*feats)[i] = nullptr;
      }
      ++i;
      continue;
    }

    if (can_batch_line(prep)) {
      size_t j = i + 1;
      while (j < feats->size() && can_batch_line((*prepared)[j]) &&
             same_line_pen(prep, (*prepared)[j])) {
        ++j;
      }
      play_line_batch(prep, i, j, /*road_batch=*/false);
      i = j;
      continue;
    }

    if (can_batch_road(prep)) {
      size_t j = i + 1;
      while (j < feats->size() && can_batch_road((*prepared)[j]) &&
             same_road_pen(prep, (*prepared)[j])) {
        ++j;
      }
      play_line_batch(prep, i, j, /*road_batch=*/true);
      i = j;
      continue;
    }

    carto_draw->feature_type() = prep.feature_type;
    carto_draw->label_priority() = prep.label_priority;
    carto_draw->is_river() = prep.is_river;
    carto_draw->road_class() = prep.road_class;
    carto_draw->anno_angle() = prep.anno_angle;
    if (prep.anno[0]) {
      strncpy_s(carto_draw->anno_buf(), 2000, prep.anno, _TRUNCATE);
    } else {
      carto_draw->anno_buf()[0] = '\0';
    }

    const bool needs_style =
        prep.kind != PrepKind::Point && prep.kind != PrepKind::Anno;
    if (needs_style && !carto_draw->lock_style()) {
      carto_draw->prepare_for_drawing(&prep.style, op);
    }

    const auto draw_begin = base::trace::Trace::time_point::clock::now();
    if (prep.kind == PrepKind::Polygon) {
      for (const PrepPart& part : prep.parts) {
        carto_draw->draw_device_polygon(part.pts.data(), part.counts.data(),
                                    static_cast<int>(part.counts.size()));
      }
    } else if (prep.kind == PrepKind::Line) {
      for (const PrepPart& part : prep.parts) {
        size_t offset = 0;
        for (int c : part.counts) {
          carto_draw->draw_device_polyline(part.pts.data() + offset, c);
          offset += static_cast<size_t>(c);
        }
      }
    } else if (prep.kind == PrepKind::Point) {
      for (const PrepPart& part : prep.parts) {
        for (const POINT& p : part.pts) {
          carto_draw->draw_device_point(static_cast<int>(p.x),
                                    static_cast<int>(p.y));
        }
      }
    } else if (prep.kind == PrepKind::Anno) {
      for (const PrepPart& part : prep.parts) {
        for (const POINT& p : part.pts) {
          carto_draw->draw_device_anno(static_cast<int>(p.x), static_cast<int>(p.y),
                                   prep.anno);
        }
      }
    }
    if (g_active_geom) {
      const int64_t us =
          std::chrono::duration_cast<std::chrono::microseconds>(
              base::trace::Trace::time_point::clock::now() - draw_begin)
              .count();
      if (prep.kind == PrepKind::Polygon) {
        g_active_geom->polygon += us;
      } else if (prep.kind == PrepKind::Line) {
        g_active_geom->line += us;
      } else if (prep.kind == PrepKind::Point) {
        g_active_geom->point += us;
      } else if (prep.kind == PrepKind::Anno) {
        g_active_geom->anno += us;
      }
    }

    if (needs_style && !carto_draw->lock_style()) {
      carto_draw->end_drawing();
    }
    // Defer DestroyFeature to end of batch â€?avoids allocator churn between
    // GDI calls (non-legacy layout also separates emit from teardown).
    (*feats)[i] = feat;
    ++i;
  }
  for (size_t i = 0; i < feats->size(); ++i) {
    if ((*prepared)[i].kind == PrepKind::Fallback &&
        mode == DrawBatchMode::GeomOnly) {
      continue;
    }
    if ((*feats)[i]) {
      OGRFeature::DestroyFeature((*feats)[i]);
      (*feats)[i] = nullptr;
    }
  }
  if (mode == DrawBatchMode::All) {
    feats->clear();
    prepared->clear();
  }
}

}  // namespace detail
}  // namespace render
