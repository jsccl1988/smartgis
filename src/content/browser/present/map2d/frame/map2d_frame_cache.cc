// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/frame/map2d_batches.h"
#include "content/browser/present/map2d/frame/map2d_tile_math.h"

#include <cmath>
#include <utility>

#include "effect/map/pass.h"
#include "gis/present/style/style_document.h"
#include "gis/vista/frame/frame.h"
#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/futures/combinators/async.h"
#include "base/memory/arena.h"
#include "base/trace/event/process_trace.h"

namespace content {

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
}

void Map2dFrameCache::invalidate() {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  has_frame_cache_ = false;
  last_present_was_interactive_ = false;
  last_present_reused_layout_ = false;
  cached_frame_ = gis::vista::MapFrame{};
  cached_fp_ = ContentFingerprint{};
  cached_cam_ = CameraKey{};
}

bool Map2dFrameCache::has_frame() const {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  return has_frame_cache_;
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

  // Monotonic scratch for this rebuild (temps / pmr consumers share TLS too).
  layout_scratch_.memory_resource->clear(1 << 20);
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(base::Arena::kInitialSize);
  }

  gis::style::StyleDocument parsed_style;
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
  if (!style) {
    gis::style::parse_style_document(gis::vista::default_carto_style_json(),
                                     &parsed_style);
  }

  gis::vista::Layout layout;
  gis::vista::LayoutInput in;
  in.view = {cam.width_px, cam.height_px, cam.min_x, cam.min_y, cam.max_x,
             cam.max_y};
  in.style = style ? style : &parsed_style;
  in.zoom = detail::zoom_from_scale(frame_->scale());
  effect::map::WindowsGlyphRasterizer windows_rasterizer;
  in.metrics = &windows_rasterizer;
  in.tiles = {};

  // async batches (Pipeline / parallel_for inside). Layout::build stays on the
  // caller so nested parallel_for (emit_fill / emit_line) owns the full pool.
  base::execution::GlobalNThreadPoolExecutor executor;
  const MapScene* scene = scene_;
  const bool use_carto = style == nullptr;
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
  cached_fp_ = fp;
  cached_cam_ = cam;
  has_frame_cache_ = true;
  ++layout_build_count_;
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
    return true;
  }
  if (camera_changed) {
    *action = PresentAction::kInteractiveReuse;
    cached_cam_ = cam;
    last_present_reused_layout_ = true;
    return true;
  }
  if (last_present_was_interactive_) {
    *action = PresentAction::kSettleRebuild;
    if (!rebuild_layout(cam, fp)) {
      return false;
    }
    last_present_reused_layout_ = false;
    return true;
  }
  *action = PresentAction::kStaticReuse;
  last_present_reused_layout_ = true;
  return true;
}

void Map2dFrameCache::note_present_outcome(PresentAction action) {
  std::lock_guard<std::recursive_mutex> lock(mu_);
  last_present_was_interactive_ =
      (action == PresentAction::kInteractiveReuse);
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
