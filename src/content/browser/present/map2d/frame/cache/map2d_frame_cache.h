// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_FRAME_CACHE_MAP2D_FRAME_CACHE_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_FRAME_CACHE_MAP2D_FRAME_CACHE_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "base/memory/arena.h"
#include "vista/component/map/ir.h"
#include "vista/component/map/layout.h"

namespace content {

class GisScene;
class ViewFrame;

// Shared MapIR cache for GPU present and GDI paint (dual-speed).
// C0: published shared_ptr slot + PresentSnapshot; std::mutex only mailbox /
// slot swap. C1 layer_slices_ deleted � settle reuses published MapIR items.
class Map2dFrameCache {
 public:
  // Out-of-line: MapIR / DrawItem vectors must be constructed and destroyed in
  // this TU. An inline default ctor in map2d_presenter.cc plus bind() here
  // _Tidy's a different sizeof (0xCD iterator proxy on BindPresenters).
  Map2dFrameCache();
  ~Map2dFrameCache();

  Map2dFrameCache(const Map2dFrameCache&) = delete;
  Map2dFrameCache& operator=(const Map2dFrameCache&) = delete;

  void bind(const GisScene* scene, const ViewFrame* frame);
  // Unconditional drop of published MapIR (bind / GPU latch reset).
  void invalidate();
  // True when a published frame exists and scene/layer fingerprint moved.
  // Camera-only changes stay false � prepare_for_present handles extent.
  bool content_differs_from_cache() const;

  // Scene identity for StaticReuse. content_hash is GisScene::content_revision
  // (atomic); feature/layer counts stay 0 — Display must not sample live store.
  struct ContentFingerprint {
    size_t feature_count = 0;
    size_t layer_count = 0;
    const void* style_ptr = nullptr;
    bool use_carto = true;
    uint64_t content_hash = 0;

    bool operator==(const ContentFingerprint& o) const {
      return feature_count == o.feature_count && layer_count == o.layer_count &&
             style_ptr == o.style_ptr && use_carto == o.use_carto &&
             content_hash == o.content_hash;
    }
  };

  struct CameraKey {
    uint32_t width_px = 0;
    uint32_t height_px = 0;
    double min_x = 0;
    double min_y = 0;
    double max_x = 0;
    double max_y = 0;
    double scale = 0;
    int zoom_bucket = 0;

    bool same_pixels(const CameraKey& o) const {
      return width_px == o.width_px && height_px == o.height_px;
    }
    bool same_camera(const CameraKey& o) const {
      return same_pixels(o) && min_x == o.min_x && min_y == o.min_y &&
             max_x == o.max_x && max_y == o.max_y && scale == o.scale &&
             zoom_bucket == o.zoom_bucket;
    }
  };

  // Dual-speed action for GPU present.
  enum class PresentAction {
    kRebuildFull,
    kInteractiveReuse,
    kSettleRebuild,
    kStaticReuse,
  };

  // T1.0 observational bits for present dual-track (see render-rhi §Present
  // dual-track ceiling). Does not drive rebuild/reuse decisions until T1.1+.
  struct PresentDamageReasons {
    static constexpr uint32_t kNone = 0;
    static constexpr uint32_t kCameraOnly = 1u << 0;
    static constexpr uint32_t kContent = 1u << 1;
    static constexpr uint32_t kSelection = 1u << 2;
    static constexpr uint32_t kShellOverlay = 1u << 3;
    static constexpr uint32_t kSizeZoom = 1u << 4;
  };

  // Lock-free handles for Display present (no public mutex()/frame()).
  struct PresentSnapshot {
    std::shared_ptr<const vista::MapIR> frame;
    CameraKey camera;
    std::shared_ptr<const std::vector<uint8_t>> hillshade_rgba;
    int hillshade_w = 0;
    int hillshade_h = 0;
    uint64_t layout_build_count = 0;
  };

  // Locks briefly, decides, stages layout on the executor when needed.
  // UI never joins; Display waits only when no published frame exists.
  bool prepare_for_present(uint32_t width_px, uint32_t height_px,
                           PresentAction* action);

  // Always rebuild when dirty; used by callers that need a settled frame.
  // GDI interactive paint should prefer prepare_for_present (dual-speed).
  // When snap_out/cam_out are set, copies the published frame under the
  // export pin so present cannot replace the export camera before the copy.
  bool ensure_full(uint32_t width_px, uint32_t height_px,
                   vista::MapIR* snap_out = nullptr,
                   CameraKey* cam_out = nullptr);

  // Call after a successful interactive present path.
  void note_present_outcome(PresentAction action);

  bool has_frame() const;
  // Copies shared_ptr handles only (no MapIR deep copy under the lock).
  bool snap(PresentSnapshot* out) const;
  std::shared_ptr<const vista::MapIR> share_frame() const;

  // Live ViewFrame → CameraKey for HWND GDI paint (pan/zoom xform).
  // Does not require a published frame; pixel size comes from the caller.
  CameraKey live_camera_key(uint32_t width_px, uint32_t height_px) const;

  // Coverage rebuild fired while the pointer was still moving. That present
  // must skip pixel labels; the quiet settle after release draws them.
  bool gesture_defer_labels() const;

  uint64_t layout_build_count() const {
    return layout_build_count_.load(std::memory_order_acquire);
  }
  bool last_present_reused_layout() const {
    return last_present_reused_layout_.load(std::memory_order_acquire);
  }
  uint32_t last_damage_reasons() const {
    return last_damage_reasons_.load(std::memory_order_acquire);
  }
  // OR extra bits (e.g. selection/shell from gpu present) into last + phase.
  void or_present_damage_reasons(uint32_t bits);

  // Host-baked DEM hillshade / heatmap density RGBA for textured rasters.
  // Soft-fails when the key is unknown or the bake is missing.
  bool load_raster(uint32_t texture_key, std::vector<uint8_t>* rgba, int* w,
                   int* h) const;
  // Zero-copy view of a baked raster (valid while the shared bake lives).
  bool borrow_raster(uint32_t texture_key, const uint8_t** rgba, int* w,
                     int* h) const;
  bool has_hillshade_underlay() const {
    return hillshade_ready_.load(std::memory_order_acquire);
  }

 private:
  struct LayoutJob;

  ContentFingerprint make_fingerprint(const GisScene* scene) const;
  CameraKey make_camera_key(const ViewFrame* frame, uint32_t width_px,
                            uint32_t height_px) const;

  // Stage L2 layout via Display mailbox (no same-tick async+get).
  void stage_frame(const CameraKey& cam, const ContentFingerprint& fp,
                   bool reuse_slices, bool force_publish,
                   PresentAction staged_action);
  // Publish completed jobs. When wait_if_empty and no published frame, wait
  // once on the mailbox cv (Display / export only � never HWND paint).
  bool drain_layout_jobs(bool wait_if_empty, PresentAction* drained_action);
  void publish_job(LayoutJob& job);
  void prefetch_hillshade();
  void record_present_damage(uint32_t reasons);

  // Sync rebuild for ensure_full / cold wait path (not HWND).
  bool rebuild_layout(const CameraKey& cam, const ContentFingerprint& fp,
                      bool reuse_slices, bool force_publish = false);
  void clear_hillshade_bake();
  void clear_heatmap_bake();

  // Mutex first: keeps offsetof stable across MapIR / vector ABI skew
  // between incremental objs (resource_deadlock_would_occur on bind).
  mutable std::mutex mu_;
  mutable std::condition_variable cv_;
  // Serializes sync tess (prepare / ensure_full). Concurrent rebuild_layout
  // used to bump live_layout_gen mid-bake and cancel each other, leaving
  // HWND prepare_for_present with no published frame at catalog size.
  std::mutex rebuild_mu_;

  const GisScene* scene_ = nullptr;
  const ViewFrame* frame_ = nullptr;

  // Published slot — worker make_shared then swap under mu_.
  std::shared_ptr<const vista::MapIR> published_frame_;
  ContentFingerprint cached_fp_;
  CameraKey cached_cam_;
  // Camera the published MapIR was tessellated for. Interactive pan updates
  // cached_cam_ without moving this, so coverage can be measured from the mesh.
  CameraKey mesh_cam_;
  bool has_frame_cache_ = false;
  bool last_present_was_interactive_ = false;
  bool gesture_defer_labels_ = false;
  // Zoom bucket changed during a gesture. Quiet settle must full-rebuild
  // slices; interactive presents keep the previous mesh and omit labels.
  bool pending_zoom_rebuild_ = false;
  // Pan past skirt or scale change during a gesture. Quiet settle must
  // refresh layout/hillshade; interactive presents omit labels and DEM.
  bool pending_gesture_settle_ = false;
  std::atomic<bool> last_present_reused_layout_{false};
  std::atomic<uint32_t> last_damage_reasons_{0};
  std::atomic<uint64_t> layout_build_count_{0};
  // Bumped on bind/invalidate/rebuild so in-flight Layout::build can see a
  // newer gen than layout_gen and the host can skip publish.
  std::atomic<uint64_t> live_layout_gen_{0};
  // Held for ensure_full (showcase MapIR export). prepare_for_present skips
  // rebuild so the export-sized camera is not overwritten mid-snap.
  std::atomic<bool> export_layout_pin_{false};
  // Last interactive present time (steady_clock). Settle waits ~200ms quiet.
  std::chrono::steady_clock::time_point last_interactive_tp_{};
  bool pending_interactive_clock_refresh_ = false;
  // Set when first china layout deferred DEM bake; next prepare rebuilds
  // with reuse of published MapIR items so hillshade_ms is not lumped into
  // cold tess.
  bool pending_hillshade_attach_ = false;

  // Hillshade RGBA as shared_ptr — PresentSnapshot copies the handle only.
  static constexpr uint32_t kHillshadeTextureKey = 0x48534844u;  // 'HSHD'
  static constexpr uint32_t kHeatmapTextureKey = 0x48454154u;    // 'HEAT'
  std::atomic<bool> hillshade_ready_{false};
  int hillshade_w_ = 0;
  int hillshade_h_ = 0;
  vista::TileSlot hillshade_slot_{};
  std::shared_ptr<const std::vector<uint8_t>> hillshade_rgba_;
  int heatmap_w_ = 0;
  int heatmap_h_ = 0;
  std::shared_ptr<const std::vector<uint8_t>> heatmap_rgba_;

  std::shared_ptr<LayoutJob> pending_job_;
  bool hillshade_prefetch_posted_ = false;

  base::Arena layout_scratch_{base::MemoryResource::Type::kMonotonicBuffer,
                              1 << 20};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_FRAME_CACHE_MAP2D_FRAME_CACHE_H_
