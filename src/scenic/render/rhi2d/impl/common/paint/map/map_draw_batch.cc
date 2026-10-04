// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/map/map_draw_batch.h"

#include <chrono>
#include <cstring>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_geom_trace.h"

namespace scenic {
namespace detail {
namespace {

using Clock = base::trace::Trace::time_point::clock;

int64_t elapsed_us(Clock::time_point t0) {
  return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                              t0)
      .count();
}

void add_kind_us(PrepKind kind, int64_t us) {
  if (!g_active_geom || us <= 0) {
    return;
  }
  switch (kind) {
    case PrepKind::Polygon:
      g_active_geom->polygon += us;
      break;
    case PrepKind::Line:
      g_active_geom->line += us;
      break;
    case PrepKind::Point:
      g_active_geom->point += us;
      break;
    case PrepKind::Anno:
      g_active_geom->anno += us;
      break;
    default:
      break;
  }
}

void bind_carto_fields(Rhi2dCartoDraw* draw, const PreparedFeature& prep,
                       bool with_anno) {
  draw->feature_type() = prep.feature_type;
  draw->label_priority() = prep.label_priority;
  draw->is_river() = prep.is_river;
  draw->road_class() = prep.road_class;
  draw->anno_angle() = prep.anno_angle;
  if (with_anno && prep.anno[0]) {
    strncpy_s(draw->anno_buf(), 2000, prep.anno, _TRUNCATE);
  } else {
    draw->anno_buf()[0] = '\0';
  }
}

// RAII prepare_for_drawing / end_drawing around a coalesced run (no-op when
// the DC style is locked, or for point/anno kinds).
struct ScopedCartoStyle {
  Rhi2dCartoDraw* draw = nullptr;

  ScopedCartoStyle(Rhi2dCartoDraw* d, const Style* style, int op, bool need) {
    if (!d || !need || d->lock_style()) {
      return;
    }
    d->prepare_for_drawing(style, op);
    draw = d;
  }

  ~ScopedCartoStyle() {
    if (draw) {
      draw->end_drawing();
    }
  }

  ScopedCartoStyle(const ScopedCartoStyle&) = delete;
  ScopedCartoStyle& operator=(const ScopedCartoStyle&) = delete;
};

struct LineScratch {
  std::vector<POINT> pts;
  std::vector<int> counts;

  void reset_and_reserve(size_t n_pts, size_t n_counts) {
    pts.clear();
    counts.clear();
    if (pts.capacity() < n_pts) {
      pts.reserve(n_pts);
    }
    if (counts.capacity() < n_counts) {
      counts.reserve(n_counts);
    }
  }
};

thread_local LineScratch g_line_scratch;

void append_line_parts(LineScratch* scratch, const PrepPart& part) {
  size_t offset = 0;
  for (int c : part.counts) {
    if (c >= 2) {
      const size_t old = scratch->pts.size();
      scratch->pts.resize(old + static_cast<size_t>(c));
      std::memcpy(scratch->pts.data() + old, part.pts.data() + offset,
                  static_cast<size_t>(c) * sizeof(POINT));
      scratch->counts.push_back(c);
    }
    offset += static_cast<size_t>(c);
  }
}

void flatten_line_run(LineScratch* scratch,
                      const std::vector<PreparedFeature>& prepared, size_t begin,
                      size_t end) {
  size_t n_pts = 0;
  size_t n_counts = 0;
  for (size_t k = begin; k < end; ++k) {
    for (const PrepPart& part : prepared[k].parts) {
      n_pts += part.pts.size();
      n_counts += part.counts.size();
    }
  }
  scratch->reset_and_reserve(n_pts, n_counts);
  for (size_t k = begin; k < end; ++k) {
    for (const PrepPart& part : prepared[k].parts) {
      append_line_parts(scratch, part);
    }
  }
}

void emit_polylines(Rhi2dCartoDraw* draw,
                    const std::vector<PreparedFeature>& prepared, size_t begin,
                    size_t end) {
  // Single feature: each PrepPart is already a PolyPolyline payload.
  if (end == begin + 1) {
    for (const PrepPart& part : prepared[begin].parts) {
      if (part.counts.empty()) {
        continue;
      }
      draw->draw_device_polylines(part.pts.data(), part.counts.data(),
                                  static_cast<int>(part.counts.size()));
    }
    return;
  }
  flatten_line_run(&g_line_scratch, prepared, begin, end);
  if (g_line_scratch.counts.empty()) {
    return;
  }
  draw->draw_device_polylines(g_line_scratch.pts.data(),
                              g_line_scratch.counts.data(),
                              static_cast<int>(g_line_scratch.counts.size()));
}

void emit_polygons(Rhi2dCartoDraw* draw,
                   const std::vector<PreparedFeature>& prepared, size_t begin,
                   size_t end) {
  for (size_t k = begin; k < end; ++k) {
    for (const PrepPart& part : prepared[k].parts) {
      if (part.counts.empty()) {
        continue;
      }
      draw->draw_device_polygon(part.pts.data(), part.counts.data(),
                                static_cast<int>(part.counts.size()));
    }
  }
}

void emit_points(Rhi2dCartoDraw* draw, const PreparedFeature& prep) {
  for (const PrepPart& part : prep.parts) {
    for (const POINT& p : part.pts) {
      draw->draw_device_point(static_cast<int>(p.x), static_cast<int>(p.y));
    }
  }
}

void emit_annos(Rhi2dCartoDraw* draw, const PreparedFeature& prep) {
  for (const PrepPart& part : prep.parts) {
    for (const POINT& p : part.pts) {
      draw->draw_device_anno(static_cast<int>(p.x), static_cast<int>(p.y),
                             prep.anno);
    }
  }
}

void emit_labeled_line(Rhi2dCartoDraw* draw, const PreparedFeature& prep) {
  for (const PrepPart& part : prep.parts) {
    size_t offset = 0;
    for (int c : part.counts) {
      draw->draw_device_polyline(part.pts.data() + offset, c);
      offset += static_cast<size_t>(c);
    }
  }
}

void play_line_run(Rhi2dCartoDraw* draw, int op,
                   const std::vector<PreparedFeature>& prepared, size_t begin,
                   size_t end, bool road_batch) {
  const PreparedFeature& head = prepared[begin];
  bind_carto_fields(draw, head, /*with_anno=*/false);
  // Roads set dual pens inside draw_device_polylines — skip style prep.
  ScopedCartoStyle style(draw, &head.style, op, !road_batch);
  const auto t0 = Clock::now();
  emit_polylines(draw, prepared, begin, end);
  add_kind_us(PrepKind::Line, elapsed_us(t0));
}

void play_polygon_run(Rhi2dCartoDraw* draw, int op,
                      const std::vector<PreparedFeature>& prepared,
                      size_t begin, size_t end) {
  const PreparedFeature& head = prepared[begin];
  bind_carto_fields(draw, head, /*with_anno=*/false);
  ScopedCartoStyle style(draw, &head.style, op, true);
  const auto t0 = Clock::now();
  emit_polygons(draw, prepared, begin, end);
  add_kind_us(PrepKind::Polygon, elapsed_us(t0));
}

void play_one(Rhi2dCartoDraw* draw, int op, const PreparedFeature& prep) {
  bind_carto_fields(draw, prep, /*with_anno=*/true);
  ScopedCartoStyle style(draw, &prep.style, op, prep.needs_style());
  const auto t0 = Clock::now();
  switch (prep.kind) {
    case PrepKind::Line:
      emit_labeled_line(draw, prep);
      break;
    case PrepKind::Point:
      emit_points(draw, prep);
      break;
    case PrepKind::Anno:
      emit_annos(draw, prep);
      break;
    default:
      break;
  }
  add_kind_us(prep.kind, elapsed_us(t0));
}

template <typename Pred>
size_t run_end(const std::vector<PreparedFeature>& prepared, size_t begin,
               Pred pred) {
  size_t j = begin + 1;
  while (j < prepared.size() && pred(prepared[j])) {
    ++j;
  }
  return j;
}

void release_played_feats(std::vector<OGRFeature*>* feats,
                          const std::vector<PreparedFeature>& prepared,
                          DrawBatchMode mode) {
  for (size_t i = 0; i < feats->size(); ++i) {
    if (prepared[i].kind == PrepKind::Fallback &&
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
  }
}

}  // namespace

void draw_prepared_batch(Rhi2dCartoDraw* carto_draw, int op,
                         std::vector<OGRFeature*>* feats,
                         std::vector<PreparedFeature>* prepared,
                         const FeatureFallbackFn& fallback, DrawBatchMode mode) {
  if (!carto_draw || !feats || !prepared) {
    return;
  }

  for (size_t i = 0; i < feats->size();) {
    const PreparedFeature& prep = (*prepared)[i];
    if (prep.kind == PrepKind::Skip) {
      ++i;
      continue;
    }
    if (prep.kind == PrepKind::Fallback) {
      if (mode == DrawBatchMode::All && fallback) {
        fallback((*feats)[i], op);
      }
      ++i;
      continue;
    }

    if (prep.is_plain_line()) {
      const size_t j = run_end(*prepared, i, [&](const PreparedFeature& p) {
        return p.is_plain_line() && prep.same_line_stroke(p);
      });
      play_line_run(carto_draw, op, *prepared, i, j, /*road_batch=*/false);
      i = j;
      continue;
    }

    if (prep.is_plain_road()) {
      const size_t j = run_end(*prepared, i, [&](const PreparedFeature& p) {
        return p.is_plain_road() && prep.same_road_stroke(p);
      });
      play_line_run(carto_draw, op, *prepared, i, j, /*road_batch=*/true);
      i = j;
      continue;
    }

    if (prep.kind == PrepKind::Polygon) {
      const size_t j = run_end(*prepared, i, [&](const PreparedFeature& p) {
        return p.kind == PrepKind::Polygon && prep.same_polygon_fill(p);
      });
      play_polygon_run(carto_draw, op, *prepared, i, j);
      i = j;
      continue;
    }

    play_one(carto_draw, op, prep);
    ++i;
  }

  release_played_feats(feats, *prepared, mode);
  if (mode == DrawBatchMode::All) {
    prepared->clear();
  }
}

}  // namespace detail
}  // namespace scenic
