// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/frame/map2d_batches.h"
#include "content/browser/present/map2d/frame/map2d_tile_math.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"

#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/arena.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "vista/pass/map/pass.h"
#include "gis/style/document/style_document.h"
#include "gis/style/eval/style_rules.h"
#include "vista/component/map/ir.h"
#include "vista/component/map/detail/hillshade_bake.h"
#include "vista/terrain/dem/dem_raster.h"

namespace content {
namespace {

// Non-owning view of layer_slices_ for one Layout::build. The map outlives it.
class MapSliceLookup final : public vista::SliceCache {
 public:
  explicit MapSliceLookup(
      const std::unordered_map<uint64_t, std::vector<vista::DrawItem>>& slices)
      : slices_(slices) {}

  const std::vector<vista::DrawItem>* find(uint64_t cache_key) const override {
    const auto found = slices_.find(cache_key);
    return found == slices_.end() ? nullptr : &found->second;
  }

 private:
  const std::unordered_map<uint64_t, std::vector<vista::DrawItem>>& slices_;
};

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

}  // namespace

void Map2dFrameCache::clear_hillshade_bake() {
  hillshade_ready_ = false;
  hillshade_w_ = 0;
  hillshade_h_ = 0;
  hillshade_rgba_.clear();
}

void Map2dFrameCache::bind(const MapScene* scene, const ViewFrame* frame) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  live_layout_gen_.fetch_add(1, std::memory_order_acq_rel);
  scene_ = scene;
  frame_ = frame;
  has_frame_cache_ = false;
  last_present_was_interactive_ = false;
  last_present_reused_layout_ = false;
  cached_frame_ = vista::MapIR{};
  layer_slices_.clear();
  cached_fp_ = ContentFingerprint{};
  cached_cam_ = CameraKey{};
  clear_hillshade_bake();
}

void Map2dFrameCache::invalidate() {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  live_layout_gen_.fetch_add(1, std::memory_order_acq_rel);
  has_frame_cache_ = false;
  last_present_was_interactive_ = false;
  last_present_reused_layout_ = false;
  cached_frame_ = vista::MapIR{};
  layer_slices_.clear();
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

  // FNV-1a over layer identity so StaticReuse survives count-stable edits and
  // still invalidates on visibility / id / endpoint swaps without a full
  // geometry walk.
  uint64_t h = 14695981039346656037ull;
  auto mix = [&h](uint64_t v) {
    h ^= v;
    h *= 1099511628211ull;
  };
  auto mix_bytes = [&](const void* p, size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; ++i) {
      mix(b[i]);
    }
  };
  for (const MapScene::Layer& layer : scene_->layers()) {
    mix_bytes(layer.id.data(), layer.id.size());
    mix(layer.visible ? 1ull : 0ull);
    mix(static_cast<uint64_t>(layer.features.size()));
    mix(static_cast<uint64_t>(layer.kind));
    if (!layer.features.empty()) {
      const MapScene::Feature& first = layer.features.front();
      const MapScene::Feature& last = layer.features.back();
      mix(static_cast<uint64_t>(first.kind));
      mix(static_cast<uint64_t>(first.points.size()));
      mix_bytes(first.id.bytes, first.id.len);
      mix(static_cast<uint64_t>(last.kind));
      mix(static_cast<uint64_t>(last.points.size()));
      mix_bytes(last.id.bytes, last.id.len);
      auto mix_coord = [&](double v) {
        mix(static_cast<uint64_t>(static_cast<int64_t>(v * 1e6)));
      };
      if (!first.points.empty()) {
        mix_coord(first.points.front().x);
        mix_coord(first.points.front().y);
      }
      if (!last.points.empty()) {
        mix_coord(last.points.back().x);
        mix_coord(last.points.back().y);
      }
    }
  }
  fp.content_hash = h;
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

void Map2dFrameCache::absorb_layer_slices(const vista::MapIR& frame) {
  layer_slices_.clear();
  for (const vista::DrawItem& item : frame.items) {
    if (item.cache_key == 0) {
      continue;
    }
    layer_slices_[item.cache_key].push_back(item);
  }
}

bool Map2dFrameCache::rebuild_layout(const CameraKey& cam,
                                     const ContentFingerprint& fp,
                                     bool reuse_slices) {
  BASE_TRACE_EVENT("layout", "map2d.layout");
  if (!scene_ || !frame_ || cam.width_px == 0 || cam.height_px == 0) {
    return false;
  }

  const auto layout_wall_t0 = std::chrono::steady_clock::now();
  int64_t hillshade_ms = 0;

  // Keep cached_frame_ published until this gen is still current after build.
  // Do not reset TLS/scratch here — Layout::build owns TLS reset, and wiping
  // arenas while published_ still aliases them AVs on the next paint.
  const uint64_t build_gen =
      live_layout_gen_.fetch_add(1, std::memory_order_acq_rel) + 1;

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
  auto style_has_carto_slots = [](const gis::style::StyleDocument* doc) {
    if (!doc) {
      return false;
    }
    for (const gis::style::StyleLayer& layer : doc->layers) {
      if (layer.source_layer == "land" || layer.source_layer == "river" ||
          layer.source_layer == "road" || layer.source_layer == "label") {
        return true;
      }
    }
    return false;
  };
  // Fingerprint treats china-city remaps as carto (style_document() null or
  // remapped). Batch path must match: use_carto when original scene style was
  // null or china-city pack. Print carto (land/river/label, no symbols) also
  // needs slot remap so area/line/point packs bind. Evaluate once.
  const bool china_city_pack = style_is_china_city_pack(style);
  const bool carto_slots = style_has_carto_slots(style);
  const bool use_carto = style == nullptr || china_city_pack || carto_slots;
  if (china_city_pack) {
    style = nullptr;
  }
  // Default carto JSON is large; parse once per process (rebuilds are frequent
  // on settle / zoom-bucket). China-city pack remaps to this same document.
  if (!style) {
    static std::once_flag carto_once;
    static gis::style::StyleDocument carto_doc;
    std::call_once(carto_once, [] {
      (void)gis::style::parse_style_document(
          vista::default_carto_style_json(), &carto_doc);
    });
    style = &carto_doc;
  }

  vista::Layout layout;
  vista::LayoutInput in;
  in.view = {cam.width_px, cam.height_px, cam.min_x, cam.min_y, cam.max_x,
             cam.max_y};
  in.style = style;
  in.zoom = detail::zoom_from_scale(frame_->scale());
  vista::WindowsGlyphRasterizer windows_rasterizer;
  in.metrics = &windows_rasterizer;
  in.tiles = {};

  // Soft-gate: MAP2D_NO_HILLSHADE=1 skips bake.
  // Product cold start (defer_china_seed) still finds china_dem.tif on disk
  // via find_sample_dem_path even with a demo-only document — that paid
  // ~0.4s HillshadeBake inside WaitFirstMapPresent. Skip until the scene
  // has China extent (real PLP/city seed). Force with MAP2D_FORCE_HILLSHADE=1
  // (harness / agents that need shade before china open).
  // Bake into locals first and install into members only after Layout::build
  // succeeds so a failed / aborted rebuild cannot leave a half-swapped
  // hillshade_rgba_.
  auto env_flag_one = [](const char* key) {
    const char* e = std::getenv(key);
    return e && e[0] == '1' && e[1] == '\0';
  };
  const bool force_hillshade =
      base::switch_is_one("map2d-force-hillshade") ||
      env_flag_one("MAP2D_FORCE_HILLSHADE");
  const bool skip_hillshade =
      base::switch_is_one("map2d-no-hillshade") ||
      env_flag_one("MAP2D_NO_HILLSHADE") ||
      (!force_hillshade && scene_ && !scene_->has_china_extent());
  std::vector<uint8_t> baked_rgba;
  int baked_w = 0;
  int baked_h = 0;
  if (!skip_hillshade) {
    if (const gis::style::StyleLayer* hs =
            find_hillshade_layer(style, in.zoom)) {
      const std::string dem_path = vista::find_sample_dem_path();
      if (dem_path.empty()) {
        std::fprintf(stderr, "map2d: hillshade skip - china_dem not found\n");
      } else {
        const auto hs_t0 = std::chrono::steady_clock::now();
        vista::HillshadeBake baked;
        {
          BASE_TRACE_EVENT("HillshadeBake", "startup");
          baked = vista::bake_hillshade_slot(dem_path, in.zoom, *hs,
                                                   kHillshadeTextureKey);
        }
        if (baked.ok && baked.width > 0 && baked.height > 0 &&
            !baked.rgba.empty()) {
          // First china layout: bake/cache DEM shade but do not emit the
          // raster DrawItem. force-GDI blit_rgba_quad (kMultiply at full
          // viewport) dominated cold ShowWindow (~4s+ Debug). Next rebuild
          // after show invalidate attaches shade. MAP2D_FORCE_HILLSHADE=1
          // keeps shade on frame 0 for harnesses that require it.
          if (force_hillshade || layout_build_count_ > 0) {
            in.hillshade_tiles.push_back(baked.slot);
          }
          baked_w = baked.width;
          baked_h = baked.height;
          baked_rgba = std::move(baked.rgba);
          std::fprintf(stderr,
                       "map2d: hillshade baked %dx%d from %s tiles=%zu "
                       "opacity=%.2f key=0x%08x defer_first=%d\n",
                       baked_w, baked_h, dem_path.c_str(),
                       in.hillshade_tiles.size(),
                       in.hillshade_tiles.empty()
                           ? 0.f
                           : in.hillshade_tiles.back().opacity,
                       in.hillshade_tiles.empty()
                           ? 0u
                           : in.hillshade_tiles.back().texture_key,
                       (force_hillshade || layout_build_count_ > 0) ? 0 : 1);
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

  // Batch build stays on the caller. A prior async+get paid thread-pool
  // spawn with no overlap (Layout::build waited immediately). Nested
  // parallel_for inside emit_fill / emit_line still owns the pool.
  const double map_scale = frame_->scale();
  vista::LayerBatchSet layer_batches;
  {
    BASE_TRACE_EVENT("batches", "map2d.layout");
    layer_batches =
        detail::visible_layer_batches(scene_->layers(), use_carto, map_scale);
  }
  {
    BASE_TRACE_EVENT("build", "map2d.layout");
    MapSliceLookup retained(layer_slices_);
    if (reuse_slices && !layer_slices_.empty()) {
      in.retained_slices = &retained;
    }
    in.layout_gen = build_gen;
    in.live_layout_gen = &live_layout_gen_;
    vista::MapIR built = layout.build(in, layer_batches.batches);
    if (live_layout_gen_.load(std::memory_order_acquire) != build_gen) {
      const int64_t wall_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - layout_wall_t0)
              .count();
      const int64_t layout_ms =
          wall_ms > hillshade_ms ? (wall_ms - hillshade_ms) : wall_ms;
      note_map2d_phase_layout(layout_ms, hillshade_ms);
      return has_frame_cache_;
    }
    cached_frame_ = std::move(built);
    absorb_layer_slices(cached_frame_);
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
    if (!rebuild_layout(cam, fp, /*reuse_slices=*/false)) {
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
    if (!rebuild_layout(cam, fp, /*reuse_slices=*/true)) {
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
  return rebuild_layout(cam, fp, /*reuse_slices=*/false);
}

}  // namespace content
