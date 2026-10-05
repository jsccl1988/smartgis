// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/frame/map2d_layout_build.h"
#include "content/browser/present/map2d/frame/map2d_tile_math.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"

#include <cmath>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

#include "vista/component/map/ir.h"

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

}  // namespace

Map2dFrameCache::Map2dFrameCache() = default;

Map2dFrameCache::~Map2dFrameCache() = default;

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
  if (!scene_ || !frame_ || cam.width_px == 0 || cam.height_px == 0) {
    return false;
  }

  const auto layout_wall_t0 = std::chrono::steady_clock::now();

  // Keep cached_frame_ published until this gen is still current after build.
  // Do not reset TLS/scratch here — Layout::build owns TLS reset, and wiping
  // arenas while cached_frame_ still aliases them AVs on the next paint.
  const uint64_t build_gen =
      live_layout_gen_.fetch_add(1, std::memory_order_acq_rel) + 1;

  MapSliceLookup retained(layer_slices_);
  detail::Map2dLayoutParams params;
  params.scene = scene_;
  params.frame = frame_;
  params.cam = cam;
  params.hillshade_ready = hillshade_ready_;
  params.layout_build_count = layout_build_count_;
  params.layout_gen = build_gen;
  params.live_layout_gen = &live_layout_gen_;
  if (reuse_slices && !layer_slices_.empty()) {
    params.retained_slices = &retained;
  }

  detail::Map2dLayoutOutput built;
  if (!detail::build_map2d_layout(params, &built)) {
    return false;
  }
  const int64_t hillshade_ms = built.hillshade_ms;
  auto note_layout = [&] {
    const int64_t wall_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - layout_wall_t0)
            .count();
    const int64_t layout_ms =
        wall_ms > hillshade_ms ? (wall_ms - hillshade_ms) : wall_ms;
    note_map2d_phase_layout(layout_ms, hillshade_ms);
  };
  if (!built.ok) {
    note_layout();
    return has_frame_cache_;
  }
  cached_frame_ = std::move(built.frame);
  absorb_layer_slices(cached_frame_);
  if (!built.baked_rgba.empty() && built.baked_w > 0 && built.baked_h > 0) {
    hillshade_rgba_ = std::move(built.baked_rgba);
    hillshade_w_ = built.baked_w;
    hillshade_h_ = built.baked_h;
    hillshade_ready_ = true;
  }
  cached_fp_ = fp;
  cached_cam_ = cam;
  has_frame_cache_ = true;
  ++layout_build_count_;
  note_layout();
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
