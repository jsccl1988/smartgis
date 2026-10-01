// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/frame/map2d_batches.h"
#include "content/browser/present/map2d/frame/map2d_tile_math.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"

#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/futures/combinators/async.h"
#include "base/memory/arena.h"
#include "base/trace/event/process_trace.h"
#include "effect/map/pass.h"
#include "gis/present/style/paint_resolve.h"
#include "gis/present/style/style_document.h"
#include "gis/present/style/style_rules.h"
#include "gis/vista/frame/frame.h"
#include "gis/vista/world/terrain/process/dem_hillshade.h"
#include "gis/vista/world/terrain/dem/dem_raster.h"

namespace content {
namespace {

const gis::style::StyleLayer* find_hillshade_layer(
    const gis::style::StyleDocument* style, double zoom) {
  if (!style) {
    return nullptr;
  }
  for (const gis::style::StyleLayer& layer : style->layers) {
    if (layer.type != gis::style::LayerType::kHillshade) {
      continue;
    }
    if (!gis::style::layer_matches_zoom(layer, zoom)) {
      continue;
    }
    return &layer;
  }
  return nullptr;
}

// Process-wide bake cache: DEM path + illumination + max_edge (viewport LOD).
// Survives Map2dFrameCache::invalidate so StaticReuse rebuilds skip shade_dem.
struct HillshadeBakeCache {
  std::mutex mu;
  std::string dem_path;
  float illumination_direction_deg = 0.f;
  float illumination_altitude_deg = 0.f;
  float exaggeration = 0.f;
  uint32_t shadow_argb = 0;
  uint32_t highlight_argb = 0;
  uint32_t accent_argb = 0;
  int max_edge = 0;
  std::vector<uint8_t> rgba;
  int w = 0;
  int h = 0;
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  bool ready = false;
};

HillshadeBakeCache& hillshade_bake_cache() {
  static HillshadeBakeCache cache;
  return cache;
}

int hillshade_max_edge_for_zoom(double zoom) {
  // Overview / china framing: downsample DEM before shade (Task 3 LOD).
  if (zoom < 4.5) {
    return 128;
  }
  if (zoom < 7.0) {
    return 192;
  }
  return 256;
}

bool hillshade_cache_lookup(const std::string& dem_path,
                            const gis::HillshadeParams& params,
                            std::vector<uint8_t>* rgba, int* w, int* h,
                            double* min_x, double* min_y, double* max_x,
                            double* max_y) {
  HillshadeBakeCache& c = hillshade_bake_cache();
  std::lock_guard<std::mutex> lock(c.mu);
  if (!c.ready || c.dem_path != dem_path || c.max_edge != params.max_edge ||
      c.illumination_direction_deg != params.illumination_direction_deg ||
      c.illumination_altitude_deg != params.illumination_altitude_deg ||
      c.exaggeration != params.exaggeration ||
      c.shadow_argb != params.shadow_argb ||
      c.highlight_argb != params.highlight_argb ||
      c.accent_argb != params.accent_argb) {
    return false;
  }
  *rgba = c.rgba;
  *w = c.w;
  *h = c.h;
  *min_x = c.min_x;
  *min_y = c.min_y;
  *max_x = c.max_x;
  *max_y = c.max_y;
  return !rgba->empty() && *w > 0 && *h > 0;
}

void hillshade_cache_store(const std::string& dem_path,
                           const gis::HillshadeParams& params,
                           std::vector<uint8_t> rgba, int w, int h,
                           double min_x, double min_y, double max_x,
                           double max_y) {
  HillshadeBakeCache& c = hillshade_bake_cache();
  std::lock_guard<std::mutex> lock(c.mu);
  c.dem_path = dem_path;
  c.illumination_direction_deg = params.illumination_direction_deg;
  c.illumination_altitude_deg = params.illumination_altitude_deg;
  c.exaggeration = params.exaggeration;
  c.shadow_argb = params.shadow_argb;
  c.highlight_argb = params.highlight_argb;
  c.accent_argb = params.accent_argb;
  c.max_edge = params.max_edge;
  c.rgba = std::move(rgba);
  c.w = w;
  c.h = h;
  c.min_x = min_x;
  c.min_y = min_y;
  c.max_x = max_x;
  c.max_y = max_y;
  c.ready = !c.rgba.empty() && w > 0 && h > 0;
}

}  // namespace

void Map2dFrameCache::clear_hillshade_bake() {
  hillshade_ready_ = false;
  hillshade_w_ = 0;
  hillshade_h_ = 0;
  hillshade_rgba_.clear();
}

void Map2dFrameCache::bind(const MapScene* scene, const ViewFrame* frame) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  scene_ = scene;
  frame_ = frame;
  has_frame_cache_ = false;
  last_present_was_interactive_ = false;
  last_present_reused_layout_ = false;
  cached_frame_ = gis::vista::MapFrame{};
  cached_fp_ = ContentFingerprint{};
  cached_cam_ = CameraKey{};
  clear_hillshade_bake();
}

void Map2dFrameCache::invalidate() {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  has_frame_cache_ = false;
  last_present_was_interactive_ = false;
  last_present_reused_layout_ = false;
  cached_frame_ = gis::vista::MapFrame{};
  cached_fp_ = ContentFingerprint{};
  cached_cam_ = CameraKey{};
  clear_hillshade_bake();
}

bool Map2dFrameCache::has_frame() const {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  return has_frame_cache_;
}

bool Map2dFrameCache::load_raster(uint32_t texture_key,
                                 std::vector<uint8_t>* rgba, int* w,
                                 int* h) const {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  if (!hillshade_ready_ || texture_key != kHillshadeTextureKey || !rgba ||
      !w || !h) {
    return false;
  }
  *rgba = hillshade_rgba_;
  *w = hillshade_w_;
  *h = hillshade_h_;
  return !rgba->empty() && *w > 0 && *h > 0;
}

Map2dFrameCache::ContentFingerprint Map2dFrameCache::make_fingerprint() const {
  ContentFingerprint fp;
  if (!scene_) {
    return fp;
  }
  fp.feature_count = scene_->feature_count();
  fp.layer_count = scene_->layer_count();
  fp.style_ptr = scene_->style_document();
  fp.use_carto = scene_->style_document() == nullptr;
  return fp;
}

Map2dFrameCache::CameraKey Map2dFrameCache::make_camera_key(
    uint32_t width_px, uint32_t height_px) const {
  CameraKey key;
  key.width_px = width_px;
  key.height_px = height_px;
  if (!frame_) {
    return key;
  }
  const content::Extent2 extent = frame_->view_world_extent(
      static_cast<int>(width_px), static_cast<int>(height_px));
  key.min_x = extent.xmin;
  key.min_y = extent.ymin;
  key.max_x = extent.xmax;
  key.max_y = extent.ymax;
  key.scale = frame_->scale();
  const double zoom = detail::zoom_from_scale(frame_->scale());
  key.zoom_bucket = static_cast<int>(std::floor(zoom));
  return key;
}

bool Map2dFrameCache::rebuild_layout(const CameraKey& cam,
                                     const ContentFingerprint& fp) {
  BASE_TRACE_EVENT("layout", "map2d.layout");
  if (!scene_ || !frame_ || cam.width_px == 0 || cam.height_px == 0) {
    return false;
  }

  const auto layout_wall_t0 = std::chrono::steady_clock::now();
  int64_t hillshade_ms = 0;

  // Drop prior frame + hillshade BEFORE resetting scratch arenas. Clearing
  // monotonic / TLS pools first left STL proxies (esp. hillshade_rgba_)
  // dangling → Debug ~vector / memcpy AV inside Widget::show paint.
  cached_frame_ = gis::vista::MapFrame{};
  clear_hillshade_bake();
  has_frame_cache_ = false;

  // Monotonic scratch for this rebuild (temps / pmr consumers share TLS too).
  layout_scratch_.memory_resource->clear(1 << 20);
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(base::Arena::kInitialSize);
  }

  const gis::style::StyleDocument* style = scene_->style_document();
  // china_city.style.json keys source-layer area/line/point (+ circle on
  // point). That disables carto_source_layer remap → cream wash, orange/black
  // point squares, no river/land slots. Product china seed and showcase
  // require default MapLibre carto (land/river/label).
  auto style_is_china_city_pack = [](const gis::style::StyleDocument* doc) {
    if (!doc || doc->layers.empty()) {
      return false;
    }
    bool has_area_or_point = false;
    bool has_land_or_river = false;
    for (const gis::style::StyleLayer& layer : doc->layers) {
      if (layer.source_layer == "land" || layer.source_layer == "river" ||
          layer.source_layer == "label") {
        has_land_or_river = true;
      }
      if (layer.source_layer == "area" || layer.source_layer == "point" ||
          layer.source_layer == "line") {
        has_area_or_point = true;
      }
    }
    return has_area_or_point && !has_land_or_river;
  };
  if (style_is_china_city_pack(style)) {
    style = nullptr;
  }
  // Default carto JSON is large; parse once per process (rebuilds are frequent
  // on settle / zoom-bucket). China-city pack remaps to this same document.
  if (!style) {
    static std::once_flag carto_once;
    static gis::style::StyleDocument carto_doc;
    std::call_once(carto_once, [] {
      (void)gis::style::parse_style_document(
          gis::vista::default_carto_style_json(), &carto_doc);
    });
    style = &carto_doc;
  }

  gis::vista::Layout layout;
  gis::vista::LayoutInput in;
  in.view = {cam.width_px, cam.height_px, cam.min_x, cam.min_y, cam.max_x,
             cam.max_y};
  in.style = style;
  in.zoom = detail::zoom_from_scale(frame_->scale());
  effect::map::WindowsGlyphRasterizer windows_rasterizer;
  in.metrics = &windows_rasterizer;
  in.tiles = {};

  // Soft-gate: SMT_MAP2D_NO_HILLSHADE=1 skips bake. Bake into locals first and
  // install into members only after Layout::build succeeds so a failed /
  // aborted rebuild cannot leave a half-swapped hillshade_rgba_.
  const char* no_hs = std::getenv("SMT_MAP2D_NO_HILLSHADE");
  const bool skip_hillshade =
      no_hs && no_hs[0] == '1' && no_hs[1] == '\0';
  std::vector<uint8_t> baked_rgba;
  int baked_w = 0;
  int baked_h = 0;
  if (!skip_hillshade) {
    if (const gis::style::StyleLayer* hs =
            find_hillshade_layer(style, in.zoom)) {
      const std::string dem_path = gis::find_sample_dem_path();
      if (dem_path.empty()) {
        std::fprintf(stderr, "map2d: hillshade skip - china_dem not found\n");
      } else {
        const auto hs_t0 = std::chrono::steady_clock::now();
        // Read Style Spec hillshade keys as POD only. Avoid ResolvedPaint /
        // fill_resolved_paint here: that path assigns STL members across the
        // gis_d ↔ content boundary and has crashed Debug CRT free_dbg when
        // layouts drifted mid-build.
        gis::HillshadeParams params;
        params.max_edge = hillshade_max_edge_for_zoom(in.zoom);
        auto paint_get = [&](const char* key) -> std::string {
          const auto it = hs->paint.find(key);
          return it == hs->paint.end() ? std::string() : it->second;
        };
        if (const std::string s =
                paint_get("hillshade-illumination-direction");
            !s.empty()) {
          params.illumination_direction_deg = std::strtof(s.c_str(), nullptr);
        }
        if (const std::string s = paint_get("hillshade-exaggeration");
            !s.empty()) {
          params.exaggeration = std::strtof(s.c_str(), nullptr);
        }
        uint32_t argb = 0;
        if (gis::style::parse_color(paint_get("hillshade-shadow-color"),
                                    &argb)) {
          params.shadow_argb = argb;
        }
        if (gis::style::parse_color(paint_get("hillshade-highlight-color"),
                                    &argb)) {
          params.highlight_argb = argb;
        }
        if (gis::style::parse_color(paint_get("hillshade-accent-color"),
                                    &argb)) {
          params.accent_argb = argb;
        }

        double dem_minx = 0, dem_miny = 0, dem_maxx = 0, dem_maxy = 0;
        bool have_bake = hillshade_cache_lookup(
            dem_path, params, &baked_rgba, &baked_w, &baked_h, &dem_minx,
            &dem_miny, &dem_maxx, &dem_maxy);
        if (!have_bake) {
          gis::DemRaster dem;
          if (!dem.load_gdal_raster(dem_path.c_str()) || dem.empty()) {
            std::fprintf(stderr,
                         "map2d: hillshade skip - DEM load failed (%s)\n",
                         dem_path.c_str());
          } else if (gis::shade_dem_rgba(dem, params, &baked_rgba, &baked_w,
                                         &baked_h) &&
                     baked_w > 0 && baked_h > 0 && !baked_rgba.empty()) {
            dem.envelope(&dem_minx, &dem_miny, &dem_maxx, &dem_maxy);
            hillshade_cache_store(dem_path, params, baked_rgba, baked_w,
                                  baked_h, dem_minx, dem_miny, dem_maxx,
                                  dem_maxy);
            // store moved? we passed by value copy - baked_rgba still valid
            have_bake = true;
          } else {
            baked_rgba.clear();
            baked_w = 0;
            baked_h = 0;
            std::fprintf(stderr,
                         "map2d: hillshade skip - shade_dem_rgba failed\n");
          }
        } else {
          std::fprintf(stderr,
                       "map2d: hillshade cache hit %dx%d max_edge=%d path=%s\n",
                       baked_w, baked_h, params.max_edge, dem_path.c_str());
        }
        if (have_bake && baked_w > 0 && baked_h > 0 && !baked_rgba.empty()) {
          // DEM geotransform is lon/lat (Y = +lat). China ViewFrame extents
          // use the same +lat axis (frame_china_map2d / apply_world_extent).
          gis::vista::TileSlot slot;
          slot.min_x = dem_minx;
          slot.max_x = dem_maxx;
          slot.min_y = dem_miny;
          slot.max_y = dem_maxy;
          // Soft multiply: enough for hillshade_gray_frac>0.02 while cream
          // (incl. darkened) still passes land_cream after scorer widen.
          slot.opacity = 0.72f;
          slot.texture_key = kHillshadeTextureKey;
          in.hillshade_tiles.push_back(slot);
          std::fprintf(stderr,
                       "map2d: hillshade baked %dx%d from %s tiles=%zu "
                       "opacity=%.2f key=0x%08x max_edge=%d\n",
                       baked_w, baked_h, dem_path.c_str(),
                       in.hillshade_tiles.size(), slot.opacity,
                       slot.texture_key, params.max_edge);
        }
        hillshade_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - hs_t0)
                           .count();
      }
    } else {
      std::fprintf(stderr,
                   "map2d: hillshade skip - no style layer @ zoom=%.2f\n",
                   in.zoom);
    }
  }

  // async batches (Pipeline / parallel_for inside). Layout::build stays on the
  // caller so nested parallel_for (emit_fill / emit_line) owns the full pool.
  base::execution::GlobalNThreadPoolExecutor executor;
  const MapScene* scene = scene_;
  // Fingerprint treats china-city remaps as carto (style_document() null or
  // remapped). Batch path must match: use_carto when original scene style was
  // null or china-city pack.
  const bool use_carto =
      scene_->style_document() == nullptr || style_is_china_city_pack(
                                                 scene_->style_document());
  const double map_scale = frame_->scale();
  detail::Map2dBatches layer_batches;
  {
    BASE_TRACE_EVENT("batches", "map2d.layout");
    auto batches_fut = base::execution::async(executor, [scene, use_carto,
                                                         map_scale]() {
      return detail::visible_layer_batches(scene->layers(), use_carto,
                                           map_scale);
    });
    layer_batches = std::move(batches_fut.get());
  }
  {
    BASE_TRACE_EVENT("build", "map2d.layout");
    if (base::MemoryResource* tls = base::tls_memory_resource()) {
      tls->clear(base::Arena::kInitialSize);
    }
    cached_frame_ = layout.build(in, layer_batches.batches);
  }
  if (!baked_rgba.empty() && baked_w > 0 && baked_h > 0) {
    hillshade_rgba_ = std::move(baked_rgba);
    hillshade_w_ = baked_w;
    hillshade_h_ = baked_h;
    hillshade_ready_ = true;
  }
  cached_fp_ = fp;
  cached_cam_ = cam;
  has_frame_cache_ = true;
  ++layout_build_count_;
  const int64_t wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - layout_wall_t0)
                              .count();
  const int64_t layout_ms =
      wall_ms > hillshade_ms ? (wall_ms - hillshade_ms) : wall_ms;
  note_map2d_phase_layout(layout_ms, hillshade_ms);
  return true;
}

bool Map2dFrameCache::prepare_for_present(uint32_t width_px, uint32_t height_px,
                                          PresentAction* action) {
  if (!action) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(mu_);
  last_present_reused_layout_ = false;
  if (!scene_ || !frame_ || width_px == 0 || height_px == 0) {
    return false;
  }

  const ContentFingerprint fp = make_fingerprint();
  const CameraKey cam = make_camera_key(width_px, height_px);

  const bool content_dirty = !has_frame_cache_ || !(fp == cached_fp_) ||
                             !cam.same_pixels(cached_cam_) ||
                             cam.zoom_bucket != cached_cam_.zoom_bucket;
  const bool camera_changed =
      has_frame_cache_ && !cam.same_camera(cached_cam_);

  if (content_dirty) {
    *action = PresentAction::kRebuildFull;
    if (!rebuild_layout(cam, fp)) {
      return false;
    }
    last_present_reused_layout_ = false;
    pending_interactive_clock_refresh_ = false;
    return true;
  }
  if (camera_changed) {
    *action = PresentAction::kInteractiveReuse;
    cached_cam_ = cam;
    last_present_reused_layout_ = true;
    pending_interactive_clock_refresh_ = true;
    return true;
  }
  if (last_present_was_interactive_) {
    // Debounce settle (~200ms quiet) so continuous pan→brief pause does not
    // thrash full layout rebuilds; matches leftover GDI settle policy.
    constexpr auto kSettleQuiet = std::chrono::milliseconds(200);
    const auto now = std::chrono::steady_clock::now();
    if (last_interactive_tp_.time_since_epoch().count() != 0 &&
        now - last_interactive_tp_ < kSettleQuiet) {
      // Keep interactive bit; do not refresh the quiet timer (else settle never
      // fires under steady InvalidateRect).
      *action = PresentAction::kInteractiveReuse;
      last_present_reused_layout_ = true;
      pending_interactive_clock_refresh_ = false;
      return true;
    }
    *action = PresentAction::kSettleRebuild;
    if (!rebuild_layout(cam, fp)) {
      return false;
    }
    last_present_reused_layout_ = false;
    pending_interactive_clock_refresh_ = false;
    return true;
  }
  *action = PresentAction::kStaticReuse;
  last_present_reused_layout_ = true;
  pending_interactive_clock_refresh_ = false;
  return true;
}

void Map2dFrameCache::note_present_outcome(PresentAction action) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  if (action == PresentAction::kInteractiveReuse) {
    if (pending_interactive_clock_refresh_ || !last_present_was_interactive_) {
      last_interactive_tp_ = std::chrono::steady_clock::now();
    }
    last_present_was_interactive_ = true;
  } else {
    last_present_was_interactive_ = false;
  }
  pending_interactive_clock_refresh_ = false;
}

bool Map2dFrameCache::ensure_full(uint32_t width_px, uint32_t height_px) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  if (!scene_ || !frame_ || width_px == 0 || height_px == 0) {
    return false;
  }
  const ContentFingerprint fp = make_fingerprint();
  const CameraKey cam = make_camera_key(width_px, height_px);
  const bool dirty = !has_frame_cache_ || !(fp == cached_fp_) ||
                     !cam.same_camera(cached_cam_);
  if (!dirty) {
    last_present_reused_layout_ = true;
    return true;
  }
  last_present_reused_layout_ = false;
  last_present_was_interactive_ = false;
  return rebuild_layout(cam, fp);
}

}  // namespace content
