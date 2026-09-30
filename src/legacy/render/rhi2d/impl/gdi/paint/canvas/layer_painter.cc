// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/gdi/paint/canvas/layer_painter.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "base/execution/pipeline/pipeline.h"
#include "base/math/simd.h"
#include "base/trace/event/process_trace.h"
#include "gis/datasource/provider/impl/ogr/codec/ogr_feature_codec.h"
#include "gis/model/envelope.h"
#include "legacy/gis/present/carto/style_api.h"
#include "legacy/render/detail/frame_pipeline.h"
#include "legacy/render/rhi2d/impl/gdi/paint/encode/command_encoder.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/carto_frame.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/device_geom.h"
#include "legacy/render/rhi2d/impl/gdi/surface/compose.h"
#include "legacy/render/rhi2d/impl/gdi/worker/raster_scheduler.h"
#include "ogrsf_frmts.h"

using namespace gis;
using namespace base;
using namespace geo;

namespace render {
namespace detail {
namespace {

struct GeomUsAccum {
  int64_t point = 0;
  int64_t line = 0;
  int64_t polygon = 0;
  int64_t multipoint = 0;
  int64_t multiline = 0;
  int64_t multipolygon = 0;
  int64_t ring = 0;
  int64_t anno = 0;
  int64_t image = 0;
};

thread_local GeomUsAccum* g_active_geom = nullptr;

void flush_geom_us(const GeomUsAccum& a) {
  if (!base::trace::tracing_enabled()) {
    return;
  }
  const auto now = base::trace::Trace::time_point::clock::now();
  auto emit = [&](const char* name, int64_t us) {
    if (us <= 0) {
      return;
    }
    base::trace::process_trace_add(name, "gdi.geom",
                                   now - std::chrono::microseconds(us), now);
  };
  emit("point", a.point);
  emit("line", a.line);
  emit("polygon", a.polygon);
  emit("multipoint", a.multipoint);
  emit("multiline", a.multiline);
  emit("multipolygon", a.multipolygon);
  emit("ring", a.ring);
  emit("anno", a.anno);
  emit("image", a.image);
}

std::string layer_trace_name(OGRLayer* layer) {
  if (!layer) {
    return "ogr";
  }
  const char* n = layer->GetName();
  return (n && n[0]) ? std::string(n) : std::string("ogr");
}

std::string layer_trace_name(const SmtLayer* layer) {
  if (!layer) {
    return "layer";
  }
  const char* n = layer->GetLayerName();
  return (n && n[0]) ? std::string(n) : std::string("layer");
}

// Parallel CPU prep (LP?DP + thin) then serial GDI play. HDC stays
// single-thread.
constexpr size_t kMinParallelFeatures = 16;
// Pipeline produce?map chunk size: one Context covers many features so queue
// overhead does not dominate china-scale layers (1k�3k feats).
constexpr size_t kPrepChunk = 48;

// Run prepare_at(i) for i in [0, job_count) via chunked Pipeline + freelist.
template <typename PrepareAt>
void run_chunked_prep_pipeline(size_t job_count, PrepareAt&& prepare_at) {
  if (job_count == 0) {
    return;
  }
  struct PrepCtx {
    size_t begin = 0;
    size_t end = 0;
  };
  std::atomic<size_t> next_chunk{0};
  const size_t n_chunks = (job_count + kPrepChunk - 1) / kPrepChunk;
  const size_t workers = (std::max)(
      size_t{1},
      (std::min)(n_chunks,
                 static_cast<size_t>(
                     (std::max)(1u, std::thread::hardware_concurrency()))));

  std::mutex pool_mu;
  std::vector<PrepCtx*> free_list;
  auto acquire = [&]() -> PrepCtx* {
    std::lock_guard<std::mutex> lock(pool_mu);
    if (!free_list.empty()) {
      PrepCtx* p = free_list.back();
      free_list.pop_back();
      return p;
    }
    return new PrepCtx();
  };
  auto release = [&](PrepCtx* p) {
    if (!p) {
      return;
    }
    std::lock_guard<std::mutex> lock(pool_mu);
    free_list.push_back(p);
  };

  using Status = base::execution::detail::Status;
  base::execution::Pipeline<PrepCtx> pipe({
      {1,
       [&](PrepCtx& ctx) -> Status {
         const size_t c = next_chunk.fetch_add(1, std::memory_order_relaxed);
         if (c >= n_chunks) {
           return Status::COMPLETE;
         }
         ctx.begin = c * kPrepChunk;
         ctx.end = (std::min)(job_count, ctx.begin + kPrepChunk);
         return Status::SUCCESS;
       }},
      {workers,
       [&](PrepCtx& ctx) -> Status {
         for (size_t i = ctx.begin; i < ctx.end; ++i) {
           prepare_at(i);
         }
         return Status::SUCCESS;
       }},
      {1, [&](PrepCtx&) -> Status { return Status::CONSUMED; }},
  });
  pipe.set_context_hooks(acquire, release);
  pipe.set_stage_names(
      {"gdi.prep.produce", "gdi.prep.map", "gdi.prep.sink"});
  pipe.run();
  pipe.wait();
  for (PrepCtx* p : free_list) {
    delete p;
  }
}

enum class PrepKind : uint8_t { Skip, Fallback, Polygon, Line, Point, Anno };

struct PrepPart {
  std::vector<POINT> pts;
  std::vector<int> counts;  // ring counts (poly) or single polyline length
};

struct PreparedFeature {
  PrepKind kind = PrepKind::Skip;
  SmtStyle style;
  int feature_type = 0;
  int label_priority = 5;
  bool is_river = false;
  int road_class = 0;
  float anno_angle = 0.f;
  char anno[2000]{};
  std::vector<PrepPart> parts;
};

bool append_projected_ring(const OGRLinearRing* ring, const LpToDp2& xform,
                           std::vector<POINT>* pts, std::vector<int>* counts) {
  if (!ring || !pts || !counts) {
    return false;
  }
  const int n = ring->getNumPoints();
  if (n < 2) {
    return false;
  }
  // Overview: subsample before LP?DP when rings are denser than ~1 vert/px.
  const int step = overview_vertex_step(n, xform.scale);
  const int sampled = (n + step - 1) / step;
  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> projected;
  xy.resize(static_cast<size_t>(sampled) * 2u);
  int n_out = 0;
  for (int i = 0; i < n; i += step, ++n_out) {
    xy[static_cast<size_t>(n_out) * 2u] = static_cast<float>(ring->getX(i));
    xy[static_cast<size_t>(n_out) * 2u + 1u] =
        static_cast<float>(ring->getY(i));
  }
  // Keep ring closed after subsample.
  if (n_out >= 2) {
    const float x0 = xy[0];
    const float y0 = xy[1];
    float& xl = xy[static_cast<size_t>(n_out - 1) * 2u];
    float& yl = xy[static_cast<size_t>(n_out - 1) * 2u + 1u];
    if (xl != x0 || yl != y0) {
      if (static_cast<int>(xy.size()) < (n_out + 1) * 2) {
        xy.resize(static_cast<size_t>(n_out + 1) * 2u);
      }
      xy[static_cast<size_t>(n_out) * 2u] = x0;
      xy[static_cast<size_t>(n_out) * 2u + 1u] = y0;
      ++n_out;
    }
  }
  projected.resize(static_cast<size_t>(n_out));
  transform_xy_batch(
      xform, std::span<const float>(xy.data(), static_cast<size_t>(n_out) * 2u),
      std::span<long>(reinterpret_cast<long*>(projected.data()),
                      static_cast<size_t>(n_out) * 2u));
  const size_t base = pts->size();
  const int kept = thin_device_ring(projected.data(), n_out, pts,
                                    overview_thin_chebyshev(xform.scale));
  if (kept < 3) {
    pts->resize(base);
    return false;
  }
  counts->push_back(kept);
  return true;
}

bool append_projected_line(const OGRLineString* line, const LpToDp2& xform,
                           PrepPart* part) {
  if (!line || !part) {
    return false;
  }
  const int n = line->getNumPoints();
  if (n < 2) {
    return false;
  }
  const int step = overview_vertex_step(n, xform.scale);
  const int sampled = (n + step - 1) / step;
  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> projected;
  xy.resize(static_cast<size_t>(sampled) * 2u);
  int n_out = 0;
  for (int i = 0; i < n; i += step, ++n_out) {
    xy[static_cast<size_t>(n_out) * 2u] = static_cast<float>(line->getX(i));
    xy[static_cast<size_t>(n_out) * 2u + 1u] =
        static_cast<float>(line->getY(i));
  }
  // Preserve last vertex.
  if (step > 1 && ((n - 1) % step) != 0) {
    if (static_cast<int>(xy.size()) < (n_out + 1) * 2) {
      xy.resize(static_cast<size_t>(n_out + 1) * 2u);
    }
    xy[static_cast<size_t>(n_out) * 2u] = static_cast<float>(line->getX(n - 1));
    xy[static_cast<size_t>(n_out) * 2u + 1u] =
        static_cast<float>(line->getY(n - 1));
    ++n_out;
  }
  projected.resize(static_cast<size_t>(n_out));
  transform_xy_batch(
      xform, std::span<const float>(xy.data(), static_cast<size_t>(n_out) * 2u),
      std::span<long>(reinterpret_cast<long*>(projected.data()),
                      static_cast<size_t>(n_out) * 2u));
  const size_t base = part->pts.size();
  const int kept =
      thin_device_polyline(projected.data(), n_out, &part->pts,
                           overview_thin_chebyshev(xform.scale));
  if (kept < 2) {
    part->pts.resize(base);
    return false;
  }
  part->counts.push_back(kept);
  return true;
}

bool prepare_polygon_part(const OGRPolygon* poly, const LpToDp2& xform,
                          PrepPart* part) {
  if (!poly || !part) {
    return false;
  }
  const OGRLinearRing* exterior = poly->getExteriorRing();
  if (!exterior ||
      !append_projected_ring(exterior, xform, &part->pts, &part->counts)) {
    return false;
  }
  // Overview: interior rings are sub-pixel / invisible �?skip hole transform.
  if (xform.scale >= 12.f) {
    const int n_holes = poly->getNumInteriorRings();
    for (int i = 0; i < n_holes; ++i) {
      const OGRLinearRing* hole = poly->getInteriorRing(i);
      if (hole) {
        (void)append_projected_ring(hole, xform, &part->pts, &part->counts);
      }
    }
  }
  return !part->counts.empty();
}

bool env_too_small_on_device(const Envelope& env, float scale) {
  const float dx = static_cast<float>((env.MaxX - env.MinX) * scale);
  const float dy = static_cast<float>((env.MaxY - env.MinY) * scale);
  // Overview: cull slightly larger noise envelopes (sub-3px blobs).
  const float min_px = scale < 12.f ? 3.f : 2.f;
  return dx < min_px && dy < min_px;
}

// Per-layer OGR field indices (schema is stable across features).
struct PrepFieldCache {
  bool warmed = false;
  int style = -1;
  int kind = -1;
  int cls = -1;
  int fill = -1;
  int stroke = -1;
  int name = -1;
  int anno = -1;
  int text = -1;
  int adcode = -1;
  int angle = -1;
};

void warm_prep_fields(OGRFeature* feature, PrepFieldCache* cache) {
  if (!feature || !cache || cache->warmed) {
    return;
  }
  cache->style = feature->GetFieldIndex("style");
  cache->kind = feature->GetFieldIndex("kind");
  cache->cls = feature->GetFieldIndex("class");
  cache->fill = feature->GetFieldIndex("fill");
  cache->stroke = feature->GetFieldIndex("stroke");
  cache->name = feature->GetFieldIndex("name");
  cache->anno = feature->GetFieldIndex("anno");
  cache->text = feature->GetFieldIndex("text");
  cache->adcode = feature->GetFieldIndex("adcode");
  cache->angle = feature->GetFieldIndex("angle");
  cache->warmed = true;
}

const char* cached_field(OGRFeature* feature, int index) {
  if (!feature || index < 0) {
    return "";
  }
  const char* v = feature->GetFieldAsString(index);
  return v ? v : "";
}

bool parse_html_rgb_local(const char* s, COLORREF* out) {
  if (!s || s[0] != '#' || !out) {
    return false;
  }
  unsigned r = 0;
  unsigned g = 0;
  unsigned b = 0;
  if (std::sscanf(s, "#%2x%2x%2x", &r, &g, &b) != 3) {
    return false;
  }
  *out = RGB(r, g, b);
  return true;
}

COLORREF cached_color(OGRFeature* feature, int index, COLORREF fallback) {
  if (!feature || index < 0) {
    return fallback;
  }
  COLORREF c = fallback;
  if (parse_html_rgb_local(feature->GetFieldAsString(index), &c)) {
    return c;
  }
  return fallback;
}

// Lightweight style for parallel prep: cached field indices, no anno infer.
void fill_prep_style(OGRFeature* feature, const PrepFieldCache& cache,
                     float fblc, SmtStyle* dst) {
  if (!dst) {
    return;
  }
  if (cache.style >= 0) {
    if (SmtStyle* owned = gis::datasource::copy_ogr_style_from_ogr(feature)) {
      *dst = *owned;
      delete owned;
      return;
    }
  }
  const char* kind = cached_field(feature, cache.kind);
  const bool river = kind[0] && (std::strcmp(kind, "river") == 0 ||
                                 std::strcmp(kind, "water") == 0);
  SmtPenDesc pen;
  pen.lPenStyle = PS_SOLID;
  if (river) {
    pen.lPenColor = cached_color(feature, cache.stroke, RGB(100, 160, 208));
    pen.fPenWidth = fblc > 0.01f ? (0.9f / fblc) : 0.14f;
  } else {
    pen.lPenColor = cached_color(feature, cache.stroke, RGB(196, 190, 176));
    pen.fPenWidth = fblc > 0.01f ? (1.15f / fblc) : 0.2f;
  }
  SmtBrushDesc brush;
  COLORREF fill = RGB(245, 243, 233);
  if (cache.fill >= 0) {
    (void)parse_html_rgb_local(feature->GetFieldAsString(cache.fill), &fill);
  }
  brush.lBrushColor = river ? RGB(163, 204, 255) : fill;
  dst->set_pen_desc(pen);
  dst->set_brush_desc(brush);
  dst->set_style_type(ST_PenDesc | ST_BrushDesc);
}

void prepare_one_feature(OGRFeature* feature, const Envelope& env_viewp,
                         const LpToDp2& xform, float fblc,
                         PrepFieldCache* fields, PreparedFeature* out) {
  out->kind = PrepKind::Skip;
  out->parts.clear();
  out->anno[0] = '\0';
  out->label_priority = 5;
  out->is_river = false;
  out->road_class = 0;
  out->anno_angle = 0.f;
  out->feature_type = SmtFeatureType::SmtFtUnknown;
  if (!feature || !out) {
    return;
  }

  PrepFieldCache local_cache;
  PrepFieldCache* cache = fields ? fields : &local_cache;
  warm_prep_fields(feature, cache);

  // Borrow feature geometry �?no decode_ogr_geometry clone/delete.
  OGRGeometry* geom = feature->GetGeometryRef();
  if (!geom || geom->IsEmpty()) {
    return;
  }

  Envelope env_feature;
  geo::copy_envelope(*geom, &env_feature);
  if (!env_feature.intersects(env_viewp)) {
    return;
  }

  const OGRwkbGeometryType type = wkbFlatten(geom->getGeometryType());
  if (type != wkbPoint && env_too_small_on_device(env_feature, xform.scale)) {
    return;
  }

  // Points / labels: project in parallel; occupancy runs on serial play.
  if (type == wkbPoint) {
    const auto* pt = static_cast<const OGRPoint*>(geom);
    float xy[2] = {static_cast<float>(pt->getX()),
                   static_cast<float>(pt->getY())};
    long dp[2] = {};
    transform_xy_batch(xform, std::span<const float>(xy, 2),
                       std::span<long>(dp, 2));
    PrepPart part;
    part.pts.push_back(POINT{dp[0], dp[1]});
    part.counts.push_back(1);
    out->parts.push_back(std::move(part));

    const char* anno_f = cached_field(feature, cache->anno);
    // Nonempty anno �?label (matches infer_feature_type); else POI disc.
    if (anno_f[0]) {
      const char* name = cached_field(feature, cache->name);
      const char* text = cached_field(feature, cache->text);
      const char* kind = cached_field(feature, cache->kind);
      const char* cls = cached_field(feature, cache->cls);
      const char* adcode = cached_field(feature, cache->adcode);
      if (const char* picked = carto2d_label_text(anno_f, name, text)) {
        strncpy_s(out->anno, picked, _TRUNCATE);
      } else {
        strncpy_s(out->anno, anno_f, _TRUNCATE);
      }
      out->label_priority =
          carto2d_label_priority(out->anno, kind, cls, adcode);
      if (out->label_priority > carto2d_lod_max_priority(fblc)) {
        out->parts.clear();
        return;  // Skip �?below LOD
      }
      if (cache->angle >= 0) {
        out->anno_angle =
            static_cast<float>(feature->GetFieldAsDouble(cache->angle));
      }
      out->feature_type = SmtFeatureType::SmtFtAnno;
      out->kind = PrepKind::Anno;
      return;
    }

    out->feature_type = SmtFeatureType::SmtFtDot;
    out->kind = PrepKind::Point;
    return;
  }
  if (type == wkbMultiPoint) {
    const auto* multi = static_cast<const OGRMultiPoint*>(geom);
    const int n = multi->getNumGeometries();
    PrepPart part;
    part.pts.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      const auto* pt = static_cast<const OGRPoint*>(multi->getGeometryRef(i));
      if (!pt) {
        continue;
      }
      float xy[2] = {static_cast<float>(pt->getX()),
                     static_cast<float>(pt->getY())};
      long dp[2] = {};
      transform_xy_batch(xform, std::span<const float>(xy, 2),
                         std::span<long>(dp, 2));
      part.pts.push_back(POINT{dp[0], dp[1]});
    }
    if (part.pts.empty()) {
      return;
    }
    part.counts.push_back(static_cast<int>(part.pts.size()));
    out->parts.push_back(std::move(part));
    out->feature_type = SmtFeatureType::SmtFtDot;
    out->kind = PrepKind::Point;
    return;
  }

  const char* kind = cached_field(feature, cache->kind);
  const char* cls = cached_field(feature, cache->cls);
  out->is_river = carto2d_is_river_kind(kind);
  out->road_class = carto2d_road_class(kind, cls);

  switch (type) {
    case wkbPolygon:
    case wkbTriangle:
    case wkbMultiPolygon:
    case wkbTIN:
      out->feature_type = SmtFeatureType::SmtFtSurface;
      break;
    case wkbLineString:
    case wkbMultiLineString:
      out->feature_type = SmtFeatureType::SmtFtCurve;
      break;
    default:
      out->kind = PrepKind::Fallback;
      return;
  }

  // Line labels only (polygons skip name/adcode/anno lookups).
  if (out->feature_type == SmtFeatureType::SmtFtCurve &&
      (out->is_river || out->road_class > 0)) {
    const char* name = cached_field(feature, cache->name);
    const char* anno = cached_field(feature, cache->anno);
    const char* text = cached_field(feature, cache->text);
    const char* adcode = cached_field(feature, cache->adcode);
    if (const char* picked = carto2d_label_text(anno, name, text)) {
      strncpy_s(out->anno, picked, _TRUNCATE);
    }
    out->label_priority = carto2d_label_priority(
        out->anno[0] ? out->anno : name, kind, cls, adcode);
  }

  fill_prep_style(feature, *cache, fblc, &out->style);

  switch (type) {
    case wkbPolygon:
    case wkbTriangle: {
      PrepPart part;
      if (prepare_polygon_part(static_cast<const OGRPolygon*>(geom), xform,
                               &part)) {
        out->parts.push_back(std::move(part));
        out->kind = PrepKind::Polygon;
      }
      break;
    }
    case wkbMultiPolygon:
    case wkbTIN: {
      const auto* multi = static_cast<const OGRMultiPolygon*>(geom);
      const int n = multi->getNumGeometries();
      for (int i = 0; i < n; ++i) {
        PrepPart part;
        if (prepare_polygon_part(
                static_cast<const OGRPolygon*>(multi->getGeometryRef(i)), xform,
                &part)) {
          out->parts.push_back(std::move(part));
        }
      }
      out->kind = out->parts.empty() ? PrepKind::Skip : PrepKind::Polygon;
      break;
    }
    case wkbLineString: {
      PrepPart part;
      if (append_projected_line(static_cast<const OGRLineString*>(geom), xform,
                                &part)) {
        out->parts.push_back(std::move(part));
        out->kind = PrepKind::Line;
      }
      break;
    }
    case wkbMultiLineString: {
      const auto* multi = static_cast<const OGRMultiLineString*>(geom);
      const int n = multi->getNumGeometries();
      PrepPart part;
      for (int i = 0; i < n; ++i) {
        (void)append_projected_line(
            static_cast<const OGRLineString*>(multi->getGeometryRef(i)), xform,
            &part);
      }
      if (!part.counts.empty()) {
        out->parts.push_back(std::move(part));
        out->kind = PrepKind::Line;
      }
      break;
    }
    default:
      out->kind = PrepKind::Fallback;
      break;
  }
}

void destroy_ogr_feats(std::vector<OGRFeature*>* feats) {
  if (!feats) {
    return;
  }
  for (OGRFeature* f : *feats) {
    OGRFeature::DestroyFeature(f);
  }
  feats->clear();
}

bool force_serial_ogr_layer(OGRLayer* layer,
                            const std::vector<OGRFeature*>& feats) {
  (void)feats;
  if (!layer) {
    return true;
  }
  // Grid / raster-tile multipoints still need the decode path; plain
  // point/anno layers prepare device coords in parallel like area/line.
  if (layer->FindFieldIndex("grid_row", TRUE) >= 0 ||
      layer->FindFieldIndex("grid_col", TRUE) >= 0) {
    return true;
  }
  return false;
}

struct OgrLayerBatch {
  OGRLayer* layer = nullptr;
  bool culled = false;
  bool force_serial = false;
  PrepFieldCache fields;
  std::vector<OGRFeature*> feats;
  std::vector<PreparedFeature> prepared;
};

using FallbackFn = std::function<void(OGRFeature*, int)>;

// GeomOnly reserved for a future parallel HDC play path.
enum class PlayMode : uint8_t { All, GeomOnly };

void play_prepared_batch(GdiPaintCanvas* canvas, int op,
                         std::vector<OGRFeature*>* feats,
                         std::vector<PreparedFeature>* prepared,
                         const FallbackFn& fallback, PlayMode mode) {
  if (!canvas || !feats || !prepared) {
    return;
  }

  // Ordinary lines (no road dual-pen, no river line label) can share one
  // PolyPolyline under a single prepare_for_drawing.
  auto can_batch_line = [](const PreparedFeature& p) {
    return p.kind == PrepKind::Line && p.road_class == 0 &&
           !(p.is_river && p.anno[0]);
  };
  auto same_line_pen = [](const PreparedFeature& a, const PreparedFeature& b) {
    return a.is_river == b.is_river && a.road_class == b.road_class &&
           a.style.get_style_type() == b.style.get_style_type();
  };

  thread_local std::vector<POINT> batch_pts;
  thread_local std::vector<int> batch_counts;

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
      if (mode == PlayMode::All) {
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

      canvas->feature_type() = prep.feature_type;
      canvas->label_priority() = prep.label_priority;
      canvas->is_river() = prep.is_river;
      canvas->road_class() = prep.road_class;
      canvas->anno_angle() = prep.anno_angle;
      canvas->anno_buf()[0] = '\0';

      if (!canvas->lock_style()) {
        canvas->prepare_for_drawing(&prep.style, op);
      }

      batch_pts.clear();
      batch_counts.clear();
      for (size_t k = i; k < j; ++k) {
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

      const auto draw_begin = base::trace::Trace::time_point::clock::now();
      if (!batch_counts.empty()) {
        canvas->draw_device_polylines(batch_pts.data(), batch_counts.data(),
                                      static_cast<int>(batch_counts.size()));
      }
      if (g_active_geom) {
        g_active_geom->line +=
            std::chrono::duration_cast<std::chrono::microseconds>(
                base::trace::Trace::time_point::clock::now() - draw_begin)
                .count();
      }

      if (!canvas->lock_style()) {
        canvas->end_drawing();
      }
      i = j;
      continue;
    }

    canvas->feature_type() = prep.feature_type;
    canvas->label_priority() = prep.label_priority;
    canvas->is_river() = prep.is_river;
    canvas->road_class() = prep.road_class;
    canvas->anno_angle() = prep.anno_angle;
    if (prep.anno[0]) {
      strncpy_s(canvas->anno_buf(), 2000, prep.anno, _TRUNCATE);
    } else {
      canvas->anno_buf()[0] = '\0';
    }

    const bool needs_style =
        prep.kind != PrepKind::Point && prep.kind != PrepKind::Anno;
    if (needs_style && !canvas->lock_style()) {
      canvas->prepare_for_drawing(&prep.style, op);
    }

    const auto draw_begin = base::trace::Trace::time_point::clock::now();
    if (prep.kind == PrepKind::Polygon) {
      for (const PrepPart& part : prep.parts) {
        canvas->draw_device_polygon(part.pts.data(), part.counts.data(),
                                    static_cast<int>(part.counts.size()));
      }
    } else if (prep.kind == PrepKind::Line) {
      for (const PrepPart& part : prep.parts) {
        size_t offset = 0;
        for (int c : part.counts) {
          canvas->draw_device_polyline(part.pts.data() + offset, c);
          offset += static_cast<size_t>(c);
        }
      }
    } else if (prep.kind == PrepKind::Point) {
      for (const PrepPart& part : prep.parts) {
        for (const POINT& p : part.pts) {
          canvas->draw_device_point(static_cast<int>(p.x),
                                    static_cast<int>(p.y));
        }
      }
    } else if (prep.kind == PrepKind::Anno) {
      for (const PrepPart& part : prep.parts) {
        for (const POINT& p : part.pts) {
          canvas->draw_device_anno(static_cast<int>(p.x), static_cast<int>(p.y),
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

    if (needs_style && !canvas->lock_style()) {
      canvas->end_drawing();
    }
    // Defer DestroyFeature to end of batch � avoids allocator churn between
    // GDI calls (non-legacy layout also separates emit from teardown).
    (*feats)[i] = feat;
    ++i;
  }
  for (size_t i = 0; i < feats->size(); ++i) {
    if ((*prepared)[i].kind == PrepKind::Fallback &&
        mode == PlayMode::GeomOnly) {
      continue;
    }
    if ((*feats)[i]) {
      OGRFeature::DestroyFeature((*feats)[i]);
      (*feats)[i] = nullptr;
    }
  }
  if (mode == PlayMode::All) {
    feats->clear();
    prepared->clear();
  }
}

}  // namespace

GdiLayerPainter::GdiLayerPainter(GdiPaintCanvas* canvas,
                                 GdiOwnedSurface* back_buf,
                                 GdiOwnedSurface* shared_front,
                                 Viewport* vir_vp1, Viewport* vir_vp2,
                                 std::mutex* shared_front_mu)
    : canvas_(canvas),
      back_buf_(back_buf),
      shared_front_(shared_front),
      vir_vp1_(vir_vp1),
      vir_vp2_(vir_vp2),
      shared_front_mu_(shared_front_mu),
      carto2d_(std::make_unique<GdiCartoFrame>()) {
  if (canvas_) {
    canvas_->set_carto(carto2d_.get());
  }
}

GdiLayerPainter::~GdiLayerPainter() = default;

void GdiLayerPainter::set_scheduler(GdiRasterScheduler* sched) {
  scheduler_ = sched;
}

void GdiLayerPainter::set_context(SmtRenderContex* rc) {
  rc_ = rc;
  if (canvas_ && rc_) {
    canvas_->set_context(rc_);
  }
}

void GdiLayerPainter::set_render_pra(const Smt2DRenderPra* pra) {
  rd_pra_ = pra;
  if (canvas_) {
    canvas_->set_render_pra(pra);
  }
}

SmtRenderContex& GdiLayerPainter::context() {
  if (rc_) {
    return *rc_;
  }
  return scheduler_->context();
}

const SmtRenderContex& GdiLayerPainter::context() const {
  if (rc_) {
    return *rc_;
  }
  return scheduler_->context();
}

void GdiLayerPainter::sync_canvas_links() {
  if (!canvas_) {
    return;
  }
  canvas_->set_carto(carto2d_.get());
  if (rc_) {
    canvas_->set_context(rc_);
  } else if (scheduler_) {
    canvas_->set_context(&scheduler_->context());
  }
  if (rd_pra_) {
    canvas_->set_render_pra(rd_pra_);
  }
}

int GdiLayerPainter::render_map(const SmtMap* map, int x, int y, int w, int h,
                                int op) {
  if (w == 0 || h == 0) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!canvas_ || !back_buf_ || !shared_front_) {
    return SMT_ERR_INVALID_PARAM;
  }

  // Capture job gen at paint start. Publish to the shared front only if this
  // generation is still current (cancel / newer resume abandons the frame).
  const uint64_t job_gen =
      scheduler_ ? scheduler_->job_generation() : uint64_t{0};
  const auto frame_aborted = [this, job_gen]() {
    return scheduler_ && scheduler_->should_abort(job_gen);
  };
  if (frame_aborted()) {
    return SMT_ERR_NONE;
  }

  sync_canvas_links();

  // Record-only map pass: Draw* ? encoder ? single replay (no mid-pass HDC).
  constexpr COLORREF kOceanClear = RGB(170, 211, 223);
  GdiCommandEncoder pass_encoder;
  pass_encoder.begin_pass(&back_buf_->surface());
  canvas_->set_encoder(&pass_encoder);
  pass_encoder.clear(kOceanClear);
  carto2d_->reset(context().fblc, w, h);

  if (map != nullptr) {
    Envelope env_viewp;
    {
      lRect l_viewp;
      fRect f_viewp;
      viewport_to_rect(l_viewp, context().viewport);
      canvas_->drect_to_lrect(l_viewp, f_viewp);
      rect_to_envelope(env_viewp, f_viewp);
    }
    const LpToDp2 xform = make_lp_to_dp(context());
    const float fblc = context().fblc;

    std::vector<OgrLayerBatch> ogr_batches;
    std::vector<const SmtLayer*> leftover_layers;
    ogr_batches.reserve(static_cast<size_t>(map->GetLayerCount()));

    for (int i = 0; i < map->GetLayerCount(); ++i) {
      if (frame_aborted()) {
        break;
      }
      if (!map->IsLayerVisible(i)) {
        continue;
      }
      if (OGRLayer* ogr = const_cast<OGRLayer*>(map->GetOgrLayer(i))) {
        OgrLayerBatch batch;
        batch.layer = ogr;
        Envelope env_layer;
        OGREnvelope ogr_env;
        if (ogr->GetExtent(&ogr_env, TRUE) == OGRERR_NONE) {
          env_layer.MinX = ogr_env.MinX;
          env_layer.MinY = ogr_env.MinY;
          env_layer.MaxX = ogr_env.MaxX;
          env_layer.MaxY = ogr_env.MaxY;
          batch.culled = !env_layer.intersects(env_viewp);
        }
        if (!batch.culled) {
          ogr->ResetReading();
          while (OGRFeature* feat = ogr->GetNextFeature()) {
            batch.feats.push_back(feat);
          }
          batch.force_serial = force_serial_ogr_layer(ogr, batch.feats);
        }
        ogr_batches.push_back(std::move(batch));
      } else if (const SmtLayer* lyr = map->GetLeftoverLayer(i)) {
        leftover_layers.push_back(lyr);
      }
    }

    // Cross-layer prep wave: area+line prepare in one parallel_for so wall time
    // is ~max(layer prep) instead of sum (HDC play stays ordered + serial).
    struct PrepJob {
      size_t batch = 0;
      size_t feat = 0;
    };
    std::vector<PrepJob> prep_jobs;
    bool any_parallel_layer = false;
    for (size_t bi = 0; bi < ogr_batches.size(); ++bi) {
      OgrLayerBatch& b = ogr_batches[bi];
      if (b.culled || b.force_serial) {
        continue;
      }
      if (b.feats.size() >= kMinParallelFeatures) {
        any_parallel_layer = true;
      }
    }
    if (any_parallel_layer) {
      for (size_t bi = 0; bi < ogr_batches.size(); ++bi) {
        OgrLayerBatch& b = ogr_batches[bi];
        if (b.culled || b.force_serial || b.feats.empty()) {
          continue;
        }
        warm_prep_fields(b.feats.front(), &b.fields);
        b.prepared.resize(b.feats.size());
        for (size_t fi = 0; fi < b.feats.size(); ++fi) {
          prep_jobs.push_back(PrepJob{bi, fi});
        }
      }
    }

    if (!prep_jobs.empty()) {
      BASE_TRACE_EVENT("prep", "gdi.map");
      log_legacy_flow("gdi.prep Pipeline run");
      run_chunked_prep_pipeline(prep_jobs.size(), [&](size_t ji) {
        const PrepJob& job = prep_jobs[ji];
        OgrLayerBatch& b = ogr_batches[job.batch];
        prepare_one_feature(b.feats[job.feat], env_viewp, xform, fblc,
                            &b.fields, &b.prepared[job.feat]);
      });
      log_legacy_flow("gdi.prep Pipeline done");
    }

    const FallbackFn fallback = [this](OGRFeature* feat, int draw_op) {
      render_feature(feat, draw_op);
    };

    // Serial ordered GDI play (HDC). Prep above is Pipeline produce?map?sink;
    // HDC play stays ordered + serial. Parallel layer play (private DCs + ocean
    // compose) regresses to AV outside the debugger � keep that cut gated.
    for (size_t bi = 0; bi < ogr_batches.size(); ++bi) {
      OgrLayerBatch& b = ogr_batches[bi];
      if (frame_aborted()) {
        destroy_ogr_feats(&b.feats);
        continue;
      }
      if (b.culled || !b.layer) {
        destroy_ogr_feats(&b.feats);
        continue;
      }
      const std::string name = layer_trace_name(b.layer);
      BASE_TRACE_EVENT(name, "gdi.layer");
      GeomUsAccum geom{};
      g_active_geom = &geom;

      if (!b.prepared.empty()) {
        BASE_TRACE_EVENT("play", "gdi.layer");
        play_prepared_batch(canvas_, op, &b.feats, &b.prepared, fallback,
                            PlayMode::All);
      } else {
        for (OGRFeature* feat : b.feats) {
          render_feature(feat, op);
          OGRFeature::DestroyFeature(feat);
        }
        b.feats.clear();
      }

      g_active_geom = nullptr;
      flush_geom_us(geom);
    }

    for (const SmtLayer* lyr : leftover_layers) {
      if (frame_aborted()) {
        break;
      }
      render_layer(lyr, op);
    }
    // Leftover tessellate is not run on the GDI worker paint path (white
    // canvas + heap overflow on large OGR packs). See gdi_renderdevice.cpp.
  }

  canvas_->flush_style();
  canvas_->set_encoder(nullptr);
  if (pass_encoder.is_recording()) {
    pass_encoder.end_pass();
  }
  {
    BASE_TRACE_EVENT("replay", "gdi.encode");
    GdiCommandBuffer recorded = pass_encoder.take_buffer();
    replay(recorded, back_buf_->surface());
  }

  // Never clear the shared map buffer on a null-map wake �?that wipes a
  // realtime ZoomToRect paint left by the UI thread. Also skip after stop /
  // cancel / superseded job: device Viewport refs may be mid-teardown, and
  // a stale blit must not overwrite the last good front.
  if (map != nullptr && !frame_aborted()) {
    std::unique_lock<std::mutex> front_lock;
    if (shared_front_mu_) {
      front_lock = std::unique_lock<std::mutex>(*shared_front_mu_);
    }
    shared_front_->clear(x, y, w, h);
    // Stretch the full map buffer �?TransparentBlt keyed on white can drop
    // near-white carto fills and leave the shared buffer empty.
    back_buf_->blit_to(*shared_front_, x, y, w, h, x, y, w, h, BLT_STRETCH,
                       SRCCOPY);

    if (vir_vp1_) {
      *vir_vp1_ = context().viewport;
    }
    if (vir_vp2_) {
      *vir_vp2_ = context().viewport;
    }
    if (scheduler_) {
      scheduler_->mark_published(job_gen);
    }

    // Phase 2: when a GPU-process sink is registered, hand off owned IR.
    // No-op when unset (leftover HWND present stays GDI).
    submit_compositor_frame(shared_front_->surface());
  }

  return SMT_ERR_NONE;
}

int GdiLayerPainter::render_layer(const SmtLayer* layer, int op) {
  if (!layer) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (layer->GetLayerType() == LYR_RASTER) {
    return render_layer(static_cast<const SmtRasterLayer*>(layer), op);
  }
  if (layer->GetLayerType() == LYR_TITLE) {
    return render_layer(static_cast<const SmtTileLayer*>(layer), op);
  }

  return SMT_ERR_FAILURE;
}

int GdiLayerPainter::render_layer(OGRLayer* layer, int op) {
  if (layer == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }
  const std::string name = layer_trace_name(layer);
  BASE_TRACE_EVENT(name, "gdi.layer");

  Envelope env_layer;
  OGREnvelope ogr_env;
  if (layer->GetExtent(&ogr_env, TRUE) == OGRERR_NONE) {
    env_layer.MinX = ogr_env.MinX;
    env_layer.MinY = ogr_env.MinY;
    env_layer.MaxX = ogr_env.MaxX;
    env_layer.MaxY = ogr_env.MaxY;
  }
  Envelope env_viewp;

  lRect l_viewp;
  fRect f_viewp;

  viewport_to_rect(l_viewp, context().viewport);
  canvas_->drect_to_lrect(l_viewp, f_viewp);
  rect_to_envelope(env_viewp, f_viewp);

  if (!env_layer.intersects(env_viewp)) {
    return SMT_ERR_NONE;
  }

  GeomUsAccum geom{};
  g_active_geom = &geom;

  std::vector<OGRFeature*> feats;
  layer->ResetReading();
  while (OGRFeature* feat = layer->GetNextFeature()) {
    feats.push_back(feat);
  }
  const bool force_serial = force_serial_ogr_layer(layer, feats);

  if (force_serial || feats.size() < kMinParallelFeatures) {
    for (OGRFeature* feat : feats) {
      render_feature(feat, op);
      OGRFeature::DestroyFeature(feat);
    }
    g_active_geom = nullptr;
    flush_geom_us(geom);
    return SMT_ERR_NONE;
  }

  const LpToDp2 xform = make_lp_to_dp(context());
  const float fblc = context().fblc;
  PrepFieldCache fields;
  warm_prep_fields(feats.front(), &fields);
  std::vector<PreparedFeature> prepared(feats.size());
  {
    BASE_TRACE_EVENT("prep", "gdi.layer");
    run_chunked_prep_pipeline(feats.size(), [&](size_t i) {
      prepare_one_feature(feats[i], env_viewp, xform, fblc, &fields,
                          &prepared[i]);
    });
  }

  {
    BASE_TRACE_EVENT("play", "gdi.layer");
    play_prepared_batch(
        canvas_, op, &feats, &prepared,
        [this](OGRFeature* feat, int draw_op) {
          render_feature(feat, draw_op);
        },
        PlayMode::All);
  }

  g_active_geom = nullptr;
  flush_geom_us(geom);

  return SMT_ERR_NONE;
}

int GdiLayerPainter::render_layer(const SmtRasterLayer* layer, int op) {
  (void)op;
  if (layer == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (!layer->IsVisible()) {
    return SMT_ERR_NONE;
  }

  const std::string name = layer_trace_name(layer);
  BASE_TRACE_EVENT(name, "gdi.layer");

  Envelope env_layer;
  layer->get_envelope(env_layer);
  Envelope env_viewp;

  lRect l_viewp;
  fRect f_viewp;

  viewport_to_rect(l_viewp, context().viewport);
  canvas_->drect_to_lrect(l_viewp, f_viewp);
  rect_to_envelope(env_viewp, f_viewp);

  if (!env_layer.intersects(env_viewp)) {
    return SMT_ERR_NONE;
  }

  char* raster_buf = nullptr;
  long raster_buf_size = 0;
  long code_type = -1;
  fRect loc_rect;

  GeomUsAccum geom{};
  g_active_geom = &geom;
  const auto t0 = base::trace::Trace::time_point::clock::now();
  if (SMT_ERR_NONE == layer->GetRasterNoClone(raster_buf, raster_buf_size,
                                              loc_rect, code_type)) {
    canvas_->stretch_image(raster_buf, raster_buf_size, loc_rect, code_type);
  }
  if (g_active_geom) {
    g_active_geom->image +=
        std::chrono::duration_cast<std::chrono::microseconds>(
            base::trace::Trace::time_point::clock::now() - t0)
            .count();
  }
  g_active_geom = nullptr;
  flush_geom_us(geom);

  return SMT_ERR_NONE;
}

int GdiLayerPainter::render_layer(const SmtTileLayer* layer, int op) {
  (void)op;
  if (layer == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (!layer->IsVisible()) {
    return SMT_ERR_NONE;
  }

  const std::string name = layer_trace_name(layer);
  BASE_TRACE_EVENT(name, "gdi.layer");

  Envelope env_layer;
  layer->get_envelope(env_layer);
  Envelope env_viewp;

  lRect l_viewp;
  fRect f_viewp;

  viewport_to_rect(l_viewp, context().viewport);
  canvas_->drect_to_lrect(l_viewp, f_viewp);
  rect_to_envelope(env_viewp, f_viewp);

  if (!env_layer.intersects(env_viewp)) {
    return SMT_ERR_NONE;
  }

  GeomUsAccum geom{};
  g_active_geom = &geom;
  const auto t0 = base::trace::Trace::time_point::clock::now();
  layer->MoveFirst();
  while (!layer->IsEnd()) {
    SmtTile* tile = layer->GetTile();
    if (tile != nullptr && tile->bVisible) {
      canvas_->stretch_image(tile->pTileBuf, tile->lTileBufSize,
                             tile->rtTileRect, tile->lImageCode);
    }

    layer->MoveNext();
  }
  if (g_active_geom) {
    g_active_geom->image +=
        std::chrono::duration_cast<std::chrono::microseconds>(
            base::trace::Trace::time_point::clock::now() - t0)
            .count();
  }
  g_active_geom = nullptr;
  flush_geom_us(geom);

  return SMT_ERR_NONE;
}

int GdiLayerPainter::render_feature(OGRFeature* feature, int op) {
  if (feature == nullptr || !canvas_) {
    return SMT_ERR_INVALID_PARAM;
  }

  canvas_->feature_type() = gis::datasource::infer_feature_type(
      feature, SmtFeatureType::SmtFtUnknown);
  SmtStyle* style = gis::datasource::copy_ogr_style_from_ogr(feature);
  const bool owned_style = style != nullptr;
  SmtStyle fallback;
  if (!style) {
    gis::datasource::fill_default_draw_style(feature, &fallback,
                                             context().fblc);
    style = &fallback;
  }
  OGRGeometry* geom = gis::datasource::decode_ogr_geometry(
      feature, static_cast<SmtFeatureType>(canvas_->feature_type()));
  char* anno = canvas_->anno_buf();
  if (canvas_->feature_type() == SmtFeatureType::SmtFtAnno) {
    const int ai = feature->GetFieldIndex("anno");
    const int gi = feature->GetFieldIndex("angle");
    if (ai >= 0) {
      const char* text = feature->GetFieldAsString(ai);
      if (text) {
        strncpy_s(anno, 2000, text, _TRUNCATE);
      }
    }
    if (gi >= 0) {
      canvas_->anno_angle() = static_cast<float>(feature->GetFieldAsDouble(gi));
    }
  }
  auto field = [feature](const char* key) -> const char* {
    if (!feature || !key) {
      return "";
    }
    const int i = feature->GetFieldIndex(key);
    if (i < 0) {
      return "";
    }
    const char* v = feature->GetFieldAsString(i);
    return v ? v : "";
  };
  const char* label =
      (canvas_->feature_type() == SmtFeatureType::SmtFtAnno && anno[0])
          ? anno
          : field("name");
  canvas_->label_priority() = carto2d_label_priority(
      label, field("kind"), field("class"), field("adcode"));
  canvas_->is_river() = carto2d_is_river_kind(field("kind"));
  canvas_->road_class() = carto2d_road_class(field("kind"), field("class"));
  if (!(canvas_->feature_type() == SmtFeatureType::SmtFtAnno && anno[0])) {
    if (const char* picked =
            carto2d_label_text(field("anno"), field("name"), field("text"))) {
      strncpy(anno, picked, 1999);
      anno[1999] = '\0';
    }
  }
  const int rc = render_geometry(geom, style, op);
  delete geom;
  if (owned_style) {
    delete style;
  }
  return rc;
}

int GdiLayerPainter::render_geometry(const OGRGeometry* geom,
                                     const SmtStyle* style, int op) {
  if (!geom || !canvas_) {
    return SMT_ERR_INVALID_PARAM;
  }

  const OGRwkbGeometryType type = wkbFlatten(geom->getGeometryType());

  Envelope env_feature, env_viewp;
  geo::copy_envelope(*geom, &env_feature);

  lRect l_viewp;
  fRect f_viewp;
  fRect fenv;
  lRect lenv;

  viewport_to_rect(l_viewp, context().viewport);
  canvas_->drect_to_lrect(l_viewp, f_viewp);
  rect_to_envelope(env_viewp, f_viewp);

  envelope_to_rect(fenv, env_feature);
  canvas_->lrect_to_drect(fenv, lenv);

  if (!env_feature.intersects(env_viewp) ||
      (type != wkbPoint && lenv.height() < 2 && lenv.width() < 2)) {
    return SMT_ERR_NONE;
  }

  HDC dc = canvas_->dc();
  const bool need_save_dc =
      rd_pra_ && rd_pra_->bShowMBR;  // MBR stroking mutates DC pen state
  if (need_save_dc) {
    ::SaveDC(dc);
  }

  if (!canvas_->lock_style()) {
    canvas_->prepare_for_drawing(style, op);
  }

  if (rd_pra_ && rd_pra_->bShowMBR) {
    long lX = 0, lY = 0;

    canvas_->lp_to_dp(env_feature.MinX, env_feature.MinY, lX, lY);
    MoveToEx(dc, lX, lY, NULL);

    canvas_->lp_to_dp(env_feature.MaxX, env_feature.MinY, lX, lY);
    LineTo(dc, lX, lY);

    canvas_->lp_to_dp(env_feature.MaxX, env_feature.MaxY, lX, lY);
    LineTo(dc, lX, lY);

    canvas_->lp_to_dp(env_feature.MinX, env_feature.MaxY, lX, lY);
    LineTo(dc, lX, lY);

    canvas_->lp_to_dp(env_feature.MinX, env_feature.MinY, lX, lY);
    LineTo(dc, lX, lY);
  }

  const auto draw_begin = base::trace::Trace::time_point::clock::now();
  switch (type) {
    case wkbPoint:
      canvas_->draw_point(style, static_cast<const OGRPoint*>(geom));
      break;
    case wkbLineString:
      canvas_->draw_line_string(static_cast<const OGRLineString*>(geom));
      break;
    case wkbPolygon:
    case wkbTriangle:
      canvas_->draw_polygon(static_cast<const OGRPolygon*>(geom));
      break;
    case wkbMultiPoint:
      canvas_->draw_multi_point(style, static_cast<const OGRMultiPoint*>(geom));
      break;
    case wkbMultiLineString:
      canvas_->draw_multi_line_string(
          static_cast<const OGRMultiLineString*>(geom));
      break;
    case wkbMultiPolygon:
    case wkbTIN:
      canvas_->draw_multi_polygon(static_cast<const OGRMultiPolygon*>(geom));
      break;
    case wkbLinearRing:
      canvas_->draw_linear_ring(static_cast<const OGRLinearRing*>(geom));
      break;
    default:
      break;
  }
  if (g_active_geom) {
    const int64_t us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            base::trace::Trace::time_point::clock::now() - draw_begin)
            .count();
    if (canvas_->feature_type() == SmtFeatureType::SmtFtAnno) {
      g_active_geom->anno += us;
    } else {
      switch (type) {
        case wkbPoint:
          g_active_geom->point += us;
          break;
        case wkbLineString:
          g_active_geom->line += us;
          break;
        case wkbPolygon:
        case wkbTriangle:
          g_active_geom->polygon += us;
          break;
        case wkbMultiPoint:
          g_active_geom->multipoint += us;
          break;
        case wkbMultiLineString:
          g_active_geom->multiline += us;
          break;
        case wkbMultiPolygon:
        case wkbTIN:
          g_active_geom->multipolygon += us;
          break;
        case wkbLinearRing:
          g_active_geom->ring += us;
          break;
        default:
          break;
      }
    }
  }
  if (!canvas_->lock_style()) {
    canvas_->end_drawing();
  }

  if (need_save_dc) {
    ::RestoreDC(dc, -1);
  }

  return SMT_ERR_NONE;
}

}  // namespace detail
}  // namespace render
