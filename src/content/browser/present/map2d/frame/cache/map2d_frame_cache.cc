// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/cache/map2d_frame_cache.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/map2d/frame/layout/map2d_layout_build.h"
#include "content/browser/present/map2d/frame/tile/map2d_tile_math.h"
#include "content/browser/present/map2d/profile/map2d_phase_profile.h"

#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/core/log.h"
#include "base/process/switches.h"
#include "vista/component/map/ir.h"
#include "vista/component/map/layout/policy/slice_key.h"

#include <cstdlib>

namespace content {
namespace {

bool env_flag_one(const char* key) {
  const char* v = std::getenv(key);
  return v && v[0] == '1' && v[1] == '\0';
}

bool force_hillshade_requested() {
  return base::switch_is_one("map2d-force-hillshade") ||
         env_flag_one("MAP2D_FORCE_HILLSHADE");
}

// Ephemeral SliceCache built from the published MapIR (C1: no layer_slices_).
class PublishedSliceLookup final : public vista::SliceCache {
 public:
  explicit PublishedSliceLookup(
      const std::shared_ptr<const vista::MapIR>& frame) {
    if (!frame) {
      return;
    }
    for (const vista::DrawItem& item : frame->items) {
      if (item.cache_key == 0) {
        continue;
      }
      by_key_[item.cache_key].push_back(item);
    }
  }

  const std::vector<vista::DrawItem>* find(uint64_t cache_key) const override {
    const auto found = by_key_.find(cache_key);
    return found == by_key_.end() ? nullptr : &found->second;
  }

 private:
  std::unordered_map<uint64_t, std::vector<vista::DrawItem>> by_key_;
};

uint32_t map2d_damage_reasons_from_state(bool has_frame, bool fp_dirty,
                                         bool zoom_dirty, bool pixels_dirty,
                                         bool camera_changed) {
  if (!has_frame) {
    return Map2dFrameCache::PresentDamageReasons::kContent;
  }
  uint32_t reasons = Map2dFrameCache::PresentDamageReasons::kNone;
  if (fp_dirty) {
    reasons |= Map2dFrameCache::PresentDamageReasons::kContent;
  }
  if (zoom_dirty || pixels_dirty) {
    reasons |= Map2dFrameCache::PresentDamageReasons::kSizeZoom;
  }
  if (camera_changed && !fp_dirty) {
    reasons |= Map2dFrameCache::PresentDamageReasons::kCameraOnly;
  }
  return reasons;
}

uint32_t map2d_damage_reasons_for_action(Map2dFrameCache::PresentAction action,
                                         uint32_t from_state) {
  if (action == Map2dFrameCache::PresentAction::kInteractiveReuse &&
      from_state == Map2dFrameCache::PresentDamageReasons::kNone) {
    return Map2dFrameCache::PresentDamageReasons::kCameraOnly;
  }
  return from_state;
}

}  // namespace

struct Map2dFrameCache::LayoutJob {
  CameraKey cam;
  ContentFingerprint fp;
  bool reuse_slices = false;
  bool force_publish = false;
  PresentAction staged_action = PresentAction::kRebuildFull;
  uint64_t build_gen = 0;
  detail::Map2dLayoutOutput built;
  std::atomic<bool> done{false};
  std::atomic<bool> ok{false};
};

Map2dFrameCache::Map2dFrameCache() = default;

Map2dFrameCache::~Map2dFrameCache() {
  live_layout_gen_.fetch_add(1, std::memory_order_acq_rel);
  // Do not join pending jobs here ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ¢ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ gen bump refuses publish; workers exit.
  std::lock_guard<std::mutex> lock(mu_);
  pending_job_.reset();
  published_frame_.reset();
  hillshade_rgba_.reset();
}

void Map2dFrameCache::clear_hillshade_bake() {
  hillshade_ready_.store(false, std::memory_order_release);
  hillshade_w_ = 0;
  hillshade_h_ = 0;
  hillshade_slot_ = {};
  hillshade_rgba_.reset();
  hillshade_prefetch_posted_ = false;
}

void Map2dFrameCache::clear_heatmap_bake() {
  heatmap_w_ = 0;
  heatmap_h_ = 0;
  heatmap_rgba_.reset();
}

void Map2dFrameCache::bind(const GisScene* scene, const ViewFrame* frame) {
  live_layout_gen_.fetch_add(1, std::memory_order_acq_rel);
  std::lock_guard<std::mutex> lock(mu_);
  scene_ = scene;
  frame_ = frame;
  has_frame_cache_ = false;
  last_present_was_interactive_ = false;
  last_present_reused_layout_.store(false, std::memory_order_release);
  pending_hillshade_attach_ = false;
  published_frame_.reset();
  pending_job_.reset();
  cached_fp_ = ContentFingerprint{};
  cached_cam_ = CameraKey{};
  mesh_cam_ = CameraKey{};
  clear_hillshade_bake();
  clear_heatmap_bake();
  cv_.notify_all();
}

void Map2dFrameCache::invalidate() {
  live_layout_gen_.fetch_add(1, std::memory_order_acq_rel);
  std::lock_guard<std::mutex> lock(mu_);
  has_frame_cache_ = false;
  last_present_was_interactive_ = false;
  last_present_reused_layout_.store(false, std::memory_order_release);
  pending_hillshade_attach_ = false;
  published_frame_.reset();
  pending_job_.reset();
  cached_fp_ = ContentFingerprint{};
  cached_cam_ = CameraKey{};
  mesh_cam_ = CameraKey{};
  clear_hillshade_bake();
  clear_heatmap_bake();
  cv_.notify_all();
}

bool Map2dFrameCache::content_differs_from_cache() const {
  const GisScene* scene = nullptr;
  ContentFingerprint cached;
  bool has = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    scene = scene_;
    cached = cached_fp_;
    has = has_frame_cache_;
  }
  if (!has || !scene) {
    return false;
  }
  return !(make_fingerprint(scene) == cached);
}

bool Map2dFrameCache::has_frame() const {
  std::lock_guard<std::mutex> lock(mu_);
  return has_frame_cache_ && published_frame_ != nullptr;
}

bool Map2dFrameCache::snap(PresentSnapshot* out) const {
  if (!out) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (!has_frame_cache_ || !published_frame_) {
    return false;
  }
  out->frame = published_frame_;
  out->camera = cached_cam_;
  out->hillshade_rgba = hillshade_rgba_;
  out->hillshade_w = hillshade_w_;
  out->hillshade_h = hillshade_h_;
  out->layout_build_count =
      layout_build_count_.load(std::memory_order_relaxed);
  return true;
}

std::shared_ptr<const vista::MapIR> Map2dFrameCache::share_frame() const {
  std::lock_guard<std::mutex> lock(mu_);
  return published_frame_;
}

bool Map2dFrameCache::load_raster(uint32_t texture_key,
                                 std::vector<uint8_t>* rgba, int* w,
                                 int* h) const {
  if (!rgba || !w || !h) {
    return false;
  }
  std::shared_ptr<const std::vector<uint8_t>> bake;
  int bw = 0;
  int bh = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (texture_key == kHillshadeTextureKey) {
      if (!hillshade_ready_.load(std::memory_order_relaxed) ||
          !hillshade_rgba_) {
        return false;
      }
      bake = hillshade_rgba_;
      bw = hillshade_w_;
      bh = hillshade_h_;
    } else if (texture_key == kHeatmapTextureKey) {
      if (!heatmap_rgba_) {
        return false;
      }
      bake = heatmap_rgba_;
      bw = heatmap_w_;
      bh = heatmap_h_;
    } else {
      return false;
    }
  }
  *rgba = *bake;
  *w = bw;
  *h = bh;
  return !rgba->empty() && *w > 0 && *h > 0;
}

bool Map2dFrameCache::borrow_raster(uint32_t texture_key, const uint8_t** rgba,
                                    int* w, int* h) const {
  // Keep the bake alive after mu_ drops. Upload reads the pointer after this
  // returns; a later publish may replace hillshade_rgba_.
  thread_local std::shared_ptr<const std::vector<uint8_t>> hold;
  std::lock_guard<std::mutex> lock(mu_);
  if (!rgba || !w || !h) {
    return false;
  }
  if (texture_key == kHillshadeTextureKey) {
    if (!hillshade_ready_.load(std::memory_order_relaxed) ||
        !hillshade_rgba_ || hillshade_rgba_->empty()) {
      return false;
    }
    hold = hillshade_rgba_;
    *rgba = hold->data();
    *w = hillshade_w_;
    *h = hillshade_h_;
    return *w > 0 && *h > 0;
  }
  if (texture_key == kHeatmapTextureKey) {
    if (!heatmap_rgba_ || heatmap_rgba_->empty()) {
      return false;
    }
    hold = heatmap_rgba_;
    *rgba = hold->data();
    *w = heatmap_w_;
    *h = heatmap_h_;
    return *w > 0 && *h > 0;
  }
  return false;
}

Map2dFrameCache::ContentFingerprint Map2dFrameCache::make_fingerprint(
    const GisScene* scene) const {
  ContentFingerprint fp;
  if (!scene) {
    return fp;
  }
  // Display prepare_for_present races UI ingest/edit. Never walk live
  // Layer/Feature vectors here (AV in make_fingerprint on endpoint mix).
  // LayerStore::content_revision bumps on layout-affecting mutations.
  fp.content_hash = scene->content_revision();
  fp.style_ptr = scene->style_document();
  fp.use_carto = fp.style_ptr == nullptr;
  // Counts are folded into content_revision; do not sample live sizes.
  fp.feature_count = 0;
  fp.layer_count = 0;
  return fp;
}

Map2dFrameCache::CameraKey Map2dFrameCache::make_camera_key(
    const ViewFrame* frame, uint32_t width_px, uint32_t height_px) const {
  CameraKey key;
  key.width_px = width_px;
  key.height_px = height_px;
  if (!frame) {
    return key;
  }
  const content::Extent2 extent = frame->view_world_extent(
      static_cast<int>(width_px), static_cast<int>(height_px));
  key.min_x = extent.xmin;
  key.min_y = extent.ymin;
  key.max_x = extent.xmax;
  key.max_y = extent.ymax;
  key.scale = frame->scale();
  const double zoom = detail::zoom_from_scale(frame->scale());
  key.zoom_bucket = static_cast<int>(std::floor(zoom));
  return key;
}

Map2dFrameCache::CameraKey Map2dFrameCache::live_camera_key(
    uint32_t width_px, uint32_t height_px) const {
  const ViewFrame* view_frame = nullptr;
  {
    std::lock_guard<std::mutex> lock(mu_);
    view_frame = frame_;
  }
  return make_camera_key(view_frame, width_px, height_px);
}

namespace {

bool camera_span_matches(const Map2dFrameCache::CameraKey& mesh,
                         const Map2dFrameCache::CameraKey& live) {
  // Prefer ViewFrame::scale(). World AABB spans change when HWND aspect
  // changes (tab / Diagnostic dock 1664x987 <-> 1664x1575) even at constant
  // map scale — comparing those spans falsely set scale_changed, forced
  // gesture_defer + SettleRebuild, and froze every pan for seconds while
  // re-compositing hillshade (reads as a sudden dark sheet).
  if (mesh.scale > 0.0 && live.scale > 0.0) {
    constexpr double kRel = 1.0e-4;
    return std::fabs(mesh.scale - live.scale) <= kRel * mesh.scale;
  }
  // Fallback: world-units-per-pixel (stable across aspect-preserving resize).
  if (mesh.width_px == 0 || live.width_px == 0) {
    return false;
  }
  const double ax = mesh.max_x - mesh.min_x;
  const double bx = live.max_x - live.min_x;
  if (!(ax > 0.0) || !(bx > 0.0)) {
    return false;
  }
  const double mesh_wpp = ax / static_cast<double>(mesh.width_px);
  const double live_wpp = bx / static_cast<double>(live.width_px);
  constexpr double kRel = 1.0e-4;
  return std::fabs(mesh_wpp - live_wpp) <= kRel * mesh_wpp;
}

double pan_pixels(const Map2dFrameCache::CameraKey& mesh,
                  const Map2dFrameCache::CameraKey& live) {
  const double span_x = mesh.max_x - mesh.min_x;
  const double span_y = mesh.max_y - mesh.min_y;
  if (!(span_x > 0.0) || !(span_y > 0.0) || live.width_px == 0 ||
      live.height_px == 0) {
    return 1.0e9;
  }
  const double dx = (live.min_x - mesh.min_x) / span_x *
                    static_cast<double>(live.width_px);
  const double dy = (live.min_y - mesh.min_y) / span_y *
                    static_cast<double>(live.height_px);
  return std::hypot(dx, dy);
}

}  // namespace

bool Map2dFrameCache::gesture_defer_labels() const {
  std::lock_guard<std::mutex> lock(mu_);
  return gesture_defer_labels_;
}

void Map2dFrameCache::publish_job(LayoutJob& job) {
  // Caller holds mu_.
  if (export_layout_pin_.load(std::memory_order_acquire) &&
      !job.force_publish) {
    return;
  }
  const uint64_t live = live_layout_gen_.load(std::memory_order_acquire);
  if (live != job.build_gen && !job.force_publish) {
    return;
  }
  if (!job.ok.load(std::memory_order_acquire) || !job.built.ok) {
    return;
  }
  if (!job.built.heatmap_rgba.empty() && job.built.heatmap_w > 0 &&
      job.built.heatmap_h > 0) {
    heatmap_w_ = job.built.heatmap_w;
    heatmap_h_ = job.built.heatmap_h;
    heatmap_rgba_ = std::make_shared<const std::vector<uint8_t>>(
        std::move(job.built.heatmap_rgba));
  } else {
    clear_heatmap_bake();
  }
  published_frame_ =
      std::make_shared<vista::MapIR>(std::move(job.built.frame));
  if (!job.built.baked_rgba.empty() && job.built.baked_w > 0 &&
      job.built.baked_h > 0) {
    // Keep bake alpha intact. MapPass upload owns one coverage scale for jet
    // HSHD; pre-crushing here made land cream dominate map2d_china_dem.
    hillshade_rgba_ = std::make_shared<const std::vector<uint8_t>>(
        std::move(job.built.baked_rgba));
    hillshade_w_ = job.built.baked_w;
    hillshade_h_ = job.built.baked_h;
    hillshade_slot_ = job.built.hillshade_slot;
    hillshade_ready_.store(true, std::memory_order_release);
    pending_hillshade_attach_ = false;
    hillshade_prefetch_posted_ = false;
  } else if (job.built.hillshade_slot.texture_key != 0) {
    hillshade_slot_ = job.built.hillshade_slot;
    pending_hillshade_attach_ = false;
  } else if (job.built.deferred_hillshade) {
    // First china layout deferred DEM bake; follow-up attach required.
    pending_hillshade_attach_ = true;
  } else {
    // Attach ran (or was soft-gated) without defer — stop ensure_full /
    // prepare settle loops from treating cream tess as "still pending".
    pending_hillshade_attach_ = false;
  }
  cached_fp_ = job.fp;
  cached_cam_ = job.cam;
  mesh_cam_ = job.cam;
  has_frame_cache_ = true;
  layout_build_count_.fetch_add(1, std::memory_order_acq_rel);
  last_present_reused_layout_.store(false, std::memory_order_release);
  cv_.notify_all();
}

void Map2dFrameCache::stage_frame(const CameraKey& cam,
                                  const ContentFingerprint& fp,
                                  bool reuse_slices, bool force_publish,
                                  PresentAction staged_action) {
  // E1 mailbox entry: post a LayoutJob then run tess on this Display tick
  // (vista Layout / GDAL DEM are not worker-safe). Never async+get. HWND
  // paint must not call stage_frame — only prepare_for_present / ensure_full.
  auto job = std::make_shared<LayoutJob>();
  job->cam = cam;
  job->fp = fp;
  job->reuse_slices = reuse_slices;
  job->force_publish = force_publish;
  job->staged_action = staged_action;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!scene_ || !frame_ || cam.width_px == 0 || cam.height_px == 0) {
      return;
    }
    if (pending_job_ && !pending_job_->done.load(std::memory_order_acquire)) {
      live_layout_gen_.fetch_add(1, std::memory_order_acq_rel);
    }
    pending_job_ = job;
  }
  // Sync rebuild publishes under mu_; mark the mailbox job done so drain
  // surfaces staged_action for G0/G1 without a second tess.
  const bool ok = rebuild_layout(cam, fp, reuse_slices, force_publish);
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (pending_job_ == job) {
      job->ok.store(ok, std::memory_order_release);
      job->done.store(true, std::memory_order_release);
      // rebuild_layout already published; drain only needs the action.
      cv_.notify_all();
    }
  }
}

bool Map2dFrameCache::drain_layout_jobs(bool wait_if_empty,
                                        PresentAction* drained_action) {
  std::unique_lock<std::mutex> lock(mu_);
  if (wait_if_empty && !has_frame_cache_) {
    cv_.wait(lock, [&] {
      return has_frame_cache_ ||
             (pending_job_ &&
              pending_job_->done.load(std::memory_order_acquire)) ||
             !pending_job_;
    });
  }
  if (!pending_job_ ||
      !pending_job_->done.load(std::memory_order_acquire)) {
    return false;
  }
  const PresentAction action = pending_job_->staged_action;
  // Worker may already have published; republish only if the slot is empty.
  if (!has_frame_cache_ && pending_job_->ok.load(std::memory_order_acquire)) {
    publish_job(*pending_job_);
  }
  pending_job_.reset();
  if (drained_action && has_frame_cache_) {
    *drained_action = action;
    return true;
  }
  return has_frame_cache_;
}

void Map2dFrameCache::prefetch_hillshade() {
  // Prefetch = stage settle rebuild on the Display mailbox (no same-tick
  // async+get). Worker-pool DEM bake stays inside Layout::build / vista.
  bool should_post = false;
  CameraKey cam;
  ContentFingerprint fp;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!pending_hillshade_attach_ || hillshade_prefetch_posted_ ||
        !has_frame_cache_ || !scene_ || !frame_) {
      return;
    }
    hillshade_prefetch_posted_ = true;
    should_post = true;
    cam = cached_cam_;
    fp = cached_fp_;
  }
  if (should_post) {
    stage_frame(cam, fp, /*reuse_slices=*/true, /*force_publish=*/false,
                PresentAction::kSettleRebuild);
  }
}

bool Map2dFrameCache::rebuild_layout(const CameraKey& cam,
                                     const ContentFingerprint& fp,
                                     bool reuse_slices, bool force_publish) {
  // Sync Display/export path: tess on this thread (never HWND). No executor
  // get() — the work runs inline, then publish under mu_.
  // Serialize so Display prepare and ensure_full cannot cancel each other
  // via live_layout_gen during a long DEM bake.
  std::lock_guard<std::mutex> rebuild_lock(rebuild_mu_);
  const GisScene* scene = nullptr;
  const ViewFrame* view_frame = nullptr;
  bool hillshade_ready = false;
  vista::TileSlot hillshade_slot{};
  uint64_t layout_build_count = 0;
  uint64_t build_gen = 0;
  std::shared_ptr<const vista::MapIR> published;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!scene_ || !frame_ || cam.width_px == 0 || cam.height_px == 0) {
      return false;
    }
    scene = scene_;
    view_frame = frame_;
    hillshade_ready = hillshade_ready_.load(std::memory_order_relaxed);
    hillshade_slot = hillshade_slot_;
    layout_build_count =
        layout_build_count_.load(std::memory_order_relaxed);
    build_gen =
        live_layout_gen_.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (reuse_slices) {
      published = published_frame_;
    }
  }

  const auto layout_wall_t0 = std::chrono::steady_clock::now();
  PublishedSliceLookup retained(published);
  detail::Map2dLayoutParams params;
  params.scene = scene;
  params.frame = view_frame;
  params.cam = cam;
  params.hillshade_ready = hillshade_ready;
  params.hillshade_slot = hillshade_slot;
  params.layout_build_count = layout_build_count;
  // Export / ensure_full force_publish must bake DEM in-frame (MAPIR dump).
  params.allow_defer_hillshade = !force_publish;
  params.layout_gen = build_gen;
  params.live_layout_gen = &live_layout_gen_;
  if (reuse_slices && published) {
    params.retained_slices = &retained;
  }

  detail::Map2dLayoutOutput built;
  if (!detail::build_map2d_layout(params, &built)) {
    return false;
  }
  const int64_t hillshade_ms = built.hillshade_ms;
  const int64_t wall_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - layout_wall_t0)
          .count();
  const int64_t layout_ms =
      wall_ms > hillshade_ms ? (wall_ms - hillshade_ms) : wall_ms;
  note_map2d_phase_layout(layout_ms, hillshade_ms);

  LayoutJob job;
  job.cam = cam;
  job.fp = fp;
  job.reuse_slices = reuse_slices;
  job.force_publish = force_publish;
  job.staged_action = force_publish ? PresentAction::kRebuildFull
                                    : PresentAction::kSettleRebuild;
  job.build_gen = build_gen;
  job.built = std::move(built);
  job.ok.store(job.built.ok, std::memory_order_release);
  job.done.store(true, std::memory_order_release);

  std::lock_guard<std::mutex> lock(mu_);
  publish_job(job);
  return has_frame_cache_;
}

void Map2dFrameCache::record_present_damage(uint32_t reasons) {
  last_damage_reasons_.store(reasons, std::memory_order_release);
  note_map2d_phase_damage(reasons);
}

void Map2dFrameCache::or_present_damage_reasons(uint32_t bits) {
  if (bits == 0) {
    return;
  }
  const uint32_t merged =
      last_damage_reasons_.fetch_or(bits, std::memory_order_acq_rel) | bits;
  note_map2d_phase_damage(merged);
}

bool Map2dFrameCache::prepare_for_present(uint32_t width_px, uint32_t height_px,
                                          PresentAction* action) {
  if (!action) {
    return false;
  }

  // Non-blocking drain of a finished worker job.
  const uint64_t builds_at_entry =
      layout_build_count_.load(std::memory_order_relaxed);
  PresentAction drained = PresentAction::kStaticReuse;
  if (drain_layout_jobs(/*wait_if_empty=*/false, &drained)) {
    const uint64_t builds_after_drain =
        layout_build_count_.load(std::memory_order_relaxed);
    const bool layout_rebuilt = builds_after_drain > builds_at_entry;
    if (layout_rebuilt) {
      *action = drained;
      last_present_reused_layout_.store(false, std::memory_order_release);
      record_present_damage(map2d_damage_reasons_for_action(
          drained, PresentDamageReasons::kContent));
    } else {
      // Sync stage_frame already published; drain only clears the mailbox.
      *action = PresentAction::kStaticReuse;
      last_present_reused_layout_.store(true, std::memory_order_release);
      record_present_damage(PresentDamageReasons::kNone);
    }
    pending_interactive_clock_refresh_ = false;
    return true;
  }

  const GisScene* scene = nullptr;
  const ViewFrame* view_frame = nullptr;
  bool has_frame = false;
  bool export_pin = false;
  bool pending_hs = false;
  bool hillshade_ready = false;
  bool last_interactive = false;
  bool job_inflight = false;
  ContentFingerprint cached_fp;
  CameraKey cached_cam;
  std::chrono::steady_clock::time_point last_interactive_tp{};
  {
    std::lock_guard<std::mutex> lock(mu_);
    scene = scene_;
    view_frame = frame_;
    has_frame = has_frame_cache_;
    export_pin = export_layout_pin_.load(std::memory_order_acquire);
    pending_hs = pending_hillshade_attach_;
    hillshade_ready = hillshade_ready_.load(std::memory_order_relaxed);
    last_interactive = last_present_was_interactive_;
    last_interactive_tp = last_interactive_tp_;
    cached_fp = cached_fp_;
    cached_cam = cached_cam_;
    job_inflight =
        pending_job_ && !pending_job_->done.load(std::memory_order_acquire);
  }

  if (!scene || !view_frame || width_px == 0 || height_px == 0) {
    return false;
  }

  // Fingerprint / camera outside the lock (C0).
  const ContentFingerprint fp = make_fingerprint(scene);
  const CameraKey cam = make_camera_key(view_frame, width_px, height_px);

  if (export_pin) {
    if (has_frame) {
      *action = PresentAction::kStaticReuse;
      last_present_reused_layout_.store(true, std::memory_order_release);
      pending_interactive_clock_refresh_ = false;
      record_present_damage(map2d_damage_reasons_for_action(
          PresentAction::kStaticReuse,
          map2d_damage_reasons_from_state(
              has_frame, !has_frame || !(fp == cached_fp),
              has_frame && cam.zoom_bucket != cached_cam.zoom_bucket,
              has_frame && !cam.same_pixels(cached_cam),
              has_frame && !cam.same_camera(cached_cam))));
      return true;
    }
    // Export holds the pin while building the first MapIR (often after
    // force_invalidate). Do not stage a competing rebuild — wait for publish
    // or pin release, then re-evaluate. Immediate false caused a 2150x1156
    // prepare_for_present fail storm on catalog HWND during dump/showcase.
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait_for(lock, std::chrono::seconds(2), [&] {
        return has_frame_cache_ ||
               !export_layout_pin_.load(std::memory_order_acquire);
      });
      has_frame = has_frame_cache_;
      export_pin = export_layout_pin_.load(std::memory_order_acquire);
      cached_fp = cached_fp_;
      cached_cam = cached_cam_;
      pending_hs = pending_hillshade_attach_;
      hillshade_ready = hillshade_ready_.load(std::memory_order_relaxed);
      job_inflight =
          pending_job_ && !pending_job_->done.load(std::memory_order_acquire);
    }
    if (export_pin) {
      if (!has_frame) {
        return false;
      }
      *action = PresentAction::kStaticReuse;
      last_present_reused_layout_.store(true, std::memory_order_release);
      pending_interactive_clock_refresh_ = false;
      record_present_damage(map2d_damage_reasons_for_action(
          PresentAction::kStaticReuse,
          map2d_damage_reasons_from_state(
              has_frame, !has_frame || !(fp == cached_fp),
              has_frame && cam.zoom_bucket != cached_cam.zoom_bucket,
              has_frame && !cam.same_pixels(cached_cam),
              has_frame && !cam.same_camera(cached_cam))));
      return true;
    }
    // Pin cleared with or without a frame — fall through for HWND-sized dirty.
  }

  // Zoom-bucket changes stay off the sync present path. A wheel notch used
  // to tessellate china inline (~seconds) and, if labels were replayed under
  // the new span, stretched halo glyphs into a white field of huge type.
  const bool fp_dirty = !has_frame || !(fp == cached_fp);
  const bool zoom_dirty =
      has_frame && cam.zoom_bucket != cached_cam.zoom_bucket;
  // HWND pixel size can flicker (tab / Diagnostic dock) without a content
  // edit. Folding that into content_dirty forced RebuildFull (china tess)
  // after every gesture that also resized the swapchain — multi-second stalls
  // and a hillshade re-composite that reads as a darker map.
  const bool pixels_dirty = has_frame && !cam.same_pixels(cached_cam);
  const bool content_dirty = fp_dirty;
  const bool camera_changed = has_frame && !cam.same_camera(cached_cam);
  const bool reuse_slices_on_rebuild =
      has_frame && !fp_dirty && !zoom_dirty;
  const uint32_t damage_reasons = map2d_damage_reasons_from_state(
      has_frame, fp_dirty, zoom_dirty, pixels_dirty, camera_changed);
  const auto record_action_damage = [this](PresentAction act,
                                           uint32_t state) {
    record_present_damage(map2d_damage_reasons_for_action(act, state));
  };

  // Timed FPS window only. Shell client pixels/zoom bucket are not the locked
  // showcase frame. Rebuilding would tessellate china for seconds and
  // overwrite cached_cam_. Content fingerprint changes still rebuild.
  if (has_frame && !fp_dirty && (pixels_dirty || zoom_dirty) &&
      base::switch_is_one("map2d-fps-bench-active")) {
    *action = PresentAction::kStaticReuse;
    last_present_reused_layout_.store(true, std::memory_order_release);
    pending_interactive_clock_refresh_ = false;
    record_action_damage(PresentAction::kStaticReuse, damage_reasons);
    return true;
  }

  if (job_inflight && has_frame) {
    // Worker still tessellating ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ¢ÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂÃÂ present published; never join.
    if (camera_changed && !content_dirty) {
      std::lock_guard<std::mutex> lock(mu_);
      cached_cam_ = cam;
      *action = PresentAction::kInteractiveReuse;
      last_present_reused_layout_.store(true, std::memory_order_release);
      pending_interactive_clock_refresh_ = true;
      record_action_damage(PresentAction::kInteractiveReuse, damage_reasons);
      return true;
    }
    *action = PresentAction::kStaticReuse;
    last_present_reused_layout_.store(true, std::memory_order_release);
    pending_interactive_clock_refresh_ = false;
    record_action_damage(PresentAction::kStaticReuse, damage_reasons);
    return true;
  }

  bool settle_rebuild = false;
  if (!content_dirty && !camera_changed) {
    if (pending_hs && !hillshade_ready) {
      // E1: hillshade attach via mailbox (Display tess; no async+get).
      const uint64_t builds_before =
          layout_build_count_.load(std::memory_order_relaxed);
      prefetch_hillshade();
      PresentAction drained = PresentAction::kSettleRebuild;
      if (!drain_layout_jobs(/*wait_if_empty=*/!has_frame, &drained) &&
          !has_frame) {
        return false;
      }
      const uint64_t builds_after =
          layout_build_count_.load(std::memory_order_relaxed);
      const bool layout_rebuilt = builds_after > builds_before;
      if (layout_rebuilt) {
        *action = drained;
        last_present_reused_layout_.store(false, std::memory_order_release);
        record_action_damage(drained, damage_reasons);
      } else {
        // Still presenting the published mesh while attach is staged.
        *action = PresentAction::kStaticReuse;
        last_present_reused_layout_.store(true, std::memory_order_release);
        record_action_damage(PresentAction::kStaticReuse, damage_reasons);
      }
      pending_interactive_clock_refresh_ = false;
      return this->has_frame();
    } else if (last_interactive) {
      constexpr auto kSettleQuiet = std::chrono::milliseconds(200);
      const auto now = std::chrono::steady_clock::now();
      if (last_interactive_tp.time_since_epoch().count() != 0 &&
          now - last_interactive_tp < kSettleQuiet) {
        *action = PresentAction::kInteractiveReuse;
        last_present_reused_layout_.store(true, std::memory_order_release);
        pending_interactive_clock_refresh_ = false;
        record_action_damage(PresentAction::kInteractiveReuse, damage_reasons);
        return true;
      }
      // In-skirt pan never deferred labels — mesh + glyphs still match the
      // camera. Skip SettleRebuild (china tess / hillshade) or every click and
      // small drag freezes the map for seconds and re-composites hillshade
      // (reads as a sudden dark sheet).
      bool need_settle = false;
      {
        std::lock_guard<std::mutex> lock(mu_);
        const bool had_label_defer = gesture_defer_labels_;
        gesture_defer_labels_ = false;
        if (pending_zoom_rebuild_) {
          need_settle = true;
        } else if (had_label_defer || pending_gesture_settle_) {
          const double moved = pan_pixels(mesh_cam_, cam);
          const double rebuild_at =
              static_cast<double>(vista::detail::kPanCoveragePx);
          // Pan-only inside the full skirt: coverage mesh still valid; show
          // labels on StaticReuse instead of SettleRebuild + emit_hillshade.
          // Scale change (wheel within bucket) always settles to re-emit DEM.
          need_settle = pending_gesture_settle_ || moved > rebuild_at;
          pending_gesture_settle_ = false;
          if (!need_settle) {
            last_present_was_interactive_ = false;
          }
        } else {
          last_present_was_interactive_ = false;
        }
      }
      if (!need_settle) {
        *action = PresentAction::kStaticReuse;
        last_present_reused_layout_.store(true, std::memory_order_release);
        pending_interactive_clock_refresh_ = false;
        record_action_damage(PresentAction::kStaticReuse, damage_reasons);
        return true;
      }
      settle_rebuild = true;
    } else {
      *action = PresentAction::kStaticReuse;
      last_present_reused_layout_.store(true, std::memory_order_release);
      pending_interactive_clock_refresh_ = false;
      record_action_damage(PresentAction::kStaticReuse, damage_reasons);
      return true;
    }
  } else if ((camera_changed || zoom_dirty) && !content_dirty) {
    CameraKey mesh;
    {
      std::lock_guard<std::mutex> lock(mu_);
      mesh = mesh_cam_;
    }
    const double moved = pan_pixels(mesh, cam);
    const bool scale_changed = !camera_span_matches(mesh, cam);
    // Inside the full pan skirt (kPanCoveragePx) at the same zoom bucket the
    // published mesh still covers the window. Float scale / aspect drift must
    // not defer labels when zoom_bucket is unchanged. Past the skirt or on
    // bucket change, present world with the live camera and omit labels until
    // quiet settle (or zoom rebuild). Sync tess here froze the map for
    // seconds; replaying layout glyphs under a mismatched span paints halos.
    const double rebuild_at =
        static_cast<double>(vista::detail::kPanCoveragePx);
    if (!zoom_dirty && !scale_changed && moved <= rebuild_at) {
      std::lock_guard<std::mutex> lock(mu_);
      cached_cam_ = cam;
      gesture_defer_labels_ = false;
      pending_gesture_settle_ = false;
      *action = PresentAction::kInteractiveReuse;
      last_present_reused_layout_.store(true, std::memory_order_release);
      pending_interactive_clock_refresh_ = true;
      record_action_damage(PresentAction::kInteractiveReuse, damage_reasons);
      return true;
    }
    bool entered_defer = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      cached_cam_ = cam;
      if (zoom_dirty) {
        pending_zoom_rebuild_ = true;
      }
      entered_defer = !gesture_defer_labels_;
      gesture_defer_labels_ = true;
      pending_gesture_settle_ = true;
    }
    if (entered_defer) {
      LOGGING(LOG_INFO,
              "map2d: gesture reuse pan_px=%.0f scale_changed=%d "
              "zoom_dirty=%d (defer labels until settle)",
              moved, scale_changed ? 1 : 0, zoom_dirty ? 1 : 0);
    }
    *action = PresentAction::kInteractiveReuse;
    last_present_reused_layout_.store(true, std::memory_order_release);
    pending_interactive_clock_refresh_ = true;
    record_action_damage(PresentAction::kInteractiveReuse, damage_reasons);
    return true;
  }

  if (content_dirty) {
    {
      std::lock_guard<std::mutex> lock(mu_);
      last_present_was_interactive_ = false;
      pending_zoom_rebuild_ = false;
      pending_gesture_settle_ = false;
    }
    // Display-tick mailbox: stage + drain for G0/G1 action. HWND never calls.
    stage_frame(cam, fp, reuse_slices_on_rebuild, /*force_publish=*/false,
                PresentAction::kRebuildFull);
    PresentAction drained = PresentAction::kRebuildFull;
    if (!drain_layout_jobs(/*wait_if_empty=*/!has_frame, &drained) &&
        !has_frame) {
      return false;
    }
    *action = PresentAction::kRebuildFull;
    last_present_reused_layout_.store(false, std::memory_order_release);
    pending_interactive_clock_refresh_ = false;
    record_action_damage(PresentAction::kRebuildFull, damage_reasons);
    return has_frame || this->has_frame();
  }

  if (settle_rebuild) {
    bool zoom_rebuild = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      zoom_rebuild = pending_zoom_rebuild_;
      pending_zoom_rebuild_ = false;
      if (!gesture_defer_labels_) {
        last_present_was_interactive_ = false;
      }
    }
    // Bucket change cannot splice the previous zoom's slices; labels and
    // zoom-gated strokes would be the wrong size.
    const bool reuse_slices = !zoom_rebuild;
    const PresentAction staged = zoom_rebuild
                                     ? PresentAction::kRebuildFull
                                     : PresentAction::kSettleRebuild;
    stage_frame(cam, fp, reuse_slices, /*force_publish=*/false, staged);
    PresentAction drained = staged;
    if (!drain_layout_jobs(/*wait_if_empty=*/!has_frame, &drained) &&
        !has_frame) {
      return false;
    }
    *action = staged;
    last_present_reused_layout_.store(false, std::memory_order_release);
    pending_interactive_clock_refresh_ = false;
    uint32_t settle_reasons = damage_reasons;
    if (zoom_rebuild) {
      settle_reasons |= PresentDamageReasons::kSizeZoom;
    }
    record_action_damage(staged, settle_reasons);
    return this->has_frame();
  }
  return false;
}

void Map2dFrameCache::note_present_outcome(PresentAction action) {
  std::lock_guard<std::mutex> lock(mu_);
  if (action == PresentAction::kInteractiveReuse) {
    if (pending_interactive_clock_refresh_ || !last_present_was_interactive_) {
      last_interactive_tp_ = std::chrono::steady_clock::now();
    }
    last_present_was_interactive_ = true;
  } else if (action == PresentAction::kSettleRebuild &&
             gesture_defer_labels_) {
    // Coverage rebuild while the pointer is still down. Leave defer set so
    // the next settle (hillshade follow-up, another pan past the skirt)
    // cannot place labels — those quads read as a white halo and oversized
    // type over a half-updated mesh. The quiet branch clears the flag.
    last_present_was_interactive_ = true;
    last_interactive_tp_ = std::chrono::steady_clock::now();
  } else {
    last_present_was_interactive_ = false;
  }
  pending_interactive_clock_refresh_ = false;
}

bool Map2dFrameCache::ensure_full(uint32_t width_px, uint32_t height_px,
                                  vista::MapIR* snap_out, CameraKey* cam_out) {
  // Present prepare targets the live viewport; showcase export asks for
  // MAP2D_SHOWCASE_W/H. Pin blocks prepare rebuilds; force-publish installs
  // the export camera; optional snap copies under the same pin.
  export_layout_pin_.store(true, std::memory_order_release);
  struct PinClear {
    Map2dFrameCache* cache = nullptr;
    ~PinClear() {
      if (!cache) {
        return;
      }
      cache->export_layout_pin_.store(false, std::memory_order_release);
      // Wake prepare_for_present waiters that blocked on pin && !has_frame.
      cache->cv_.notify_all();
    }
  } clear{this};

  bool want_forced_dem = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    want_forced_dem = force_hillshade_requested() && scene_ &&
                      scene_->has_resolvable_dem();
  }

  constexpr int kMaxAttempts = 8;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const GisScene* scene = nullptr;
    const ViewFrame* view_frame = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      scene = scene_;
      view_frame = frame_;
      if (!scene || !view_frame || width_px == 0 || height_px == 0) {
        return false;
      }
    }
    const ContentFingerprint fp = make_fingerprint(scene);
    const CameraKey cam = make_camera_key(view_frame, width_px, height_px);
    bool need_hillshade_attach = false;
    bool reuse_slices = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      // Cold china defers DEM on layout_build_count==0. export_bmp / MAPIR
      // dump must not treat that cream tess as a complete face — otherwise
      // bare DefaultChina equals H0 (no hillshade) vs gold blue DEM sheet.
      const bool hs_ready = hillshade_ready_.load(std::memory_order_relaxed);
      need_hillshade_attach =
          (pending_hillshade_attach_ || (want_forced_dem && !hs_ready)) &&
          !hs_ready;
      const bool fp_or_cam_dirty = !has_frame_cache_ || !(fp == cached_fp_) ||
                                   !cam.same_camera(cached_cam_);
      const bool dirty = fp_or_cam_dirty || need_hillshade_attach;
      if (!dirty && published_frame_ &&
          !(want_forced_dem && !hs_ready)) {
        last_present_reused_layout_.store(true, std::memory_order_release);
        if (snap_out) {
          *snap_out = *published_frame_;
        }
        if (cam_out) {
          *cam_out = cached_cam_;
        }
        return true;
      }
      // Follow-up attach reuses tess slices; camera/content dirty rebuilds.
      reuse_slices = has_frame_cache_ && !fp_or_cam_dirty && need_hillshade_attach;
      last_present_reused_layout_.store(false, std::memory_order_release);
      last_present_was_interactive_ = false;
    }
    (void)rebuild_layout(cam, fp, reuse_slices, /*force_publish=*/true);
    {
      std::lock_guard<std::mutex> lock(mu_);
      const bool hs_ready = hillshade_ready_.load(std::memory_order_relaxed);
      const bool still_pending_hs =
          (pending_hillshade_attach_ || (want_forced_dem && !hs_ready)) &&
          !hs_ready;
      if (has_frame_cache_ && published_frame_ && fp == cached_fp_ &&
          cam.same_camera(cached_cam_) && !still_pending_hs) {
        if (snap_out) {
          *snap_out = *published_frame_;
        }
        if (cam_out) {
          *cam_out = cached_cam_;
        }
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1 + attempt));
  }
  // Forced DEM showcase must not accept a cream-only snap fallback.
  if (want_forced_dem &&
      !hillshade_ready_.load(std::memory_order_relaxed)) {
    LOGGING(LOG_ERROR,
            "map2d: ensure_full forced DEM but hillshade not ready");
    return false;
  }
  PresentSnapshot snap;
  if (this->snap(&snap) && snap.frame) {
    if (snap_out) {
      *snap_out = *snap.frame;
    }
    if (cam_out) {
      *cam_out = snap.camera;
    }
    return true;
  }
  return false;
}

}  // namespace content
