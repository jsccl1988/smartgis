// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_FRAME_CACHE_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_FRAME_CACHE_H_

#include <atomic>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "base/memory/arena.h"
#include "vista/component/map/ir.h"
#include "vista/component/map/layout.h"

namespace content {

class GisScene;
class ViewFrame;

// Shared MapIR cache for GPU present and GDI paint (dual-speed §Perf).
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
  // Camera-only changes stay false — prepare_for_present handles extent.
  bool content_differs_from_cache() const;

  // Scene identity for StaticReuse. Counts alone miss visibility toggles and
  // same-count feature swaps; content_hash covers id/visible/count/endpoints.
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

  // Dual-speed action for GPU present (must hold lock via prepare/rebuild).
  enum class PresentAction {
    kRebuildFull,
    kInteractiveReuse,
    kSettleRebuild,
    kStaticReuse,
  };

  // Locks, decides, rebuilds when needed. Returns false if layout failed.
  bool prepare_for_present(uint32_t width_px, uint32_t height_px,
                           PresentAction* action);

  // Always rebuild when dirty; used by callers that need a settled frame.
  // GDI interactive paint should prefer prepare_for_present (dual-speed).
  bool ensure_full(uint32_t width_px, uint32_t height_px);

  // Call after a successful interactive present path.
  void note_present_outcome(PresentAction action);

  bool has_frame() const;
  const vista::MapIR& frame() const { return cached_frame_; }
  const CameraKey& camera() const { return cached_cam_; }
  uint64_t layout_build_count() const { return layout_build_count_; }
  bool last_present_reused_layout() const {
    return last_present_reused_layout_;
  }

  // Host-baked DEM hillshade RGBA for Pass / GDI textured raster DrawItems.
  // Soft-fails (returns false) when Style has no hillshade or DEM is missing.
  bool load_raster(uint32_t texture_key, std::vector<uint8_t>* rgba, int* w,
                   int* h) const;
  // Zero-copy view of the baked hillshade (valid while mu_ / bake live).
  bool borrow_raster(uint32_t texture_key, const uint8_t** rgba, int* w,
                     int* h) const;
  bool has_hillshade_underlay() const { return hillshade_ready_; }

  // Layout tess runs unlocked (see prepare_for_present). GPU present snaps
  // MapIR under a short lock then records without holding mu_.
  std::recursive_mutex& mutex() { return mu_; }

 private:
  ContentFingerprint make_fingerprint() const;
  CameraKey make_camera_key(uint32_t width_px, uint32_t height_px) const;
  // reuse_slices keeps DrawItem groups whose cache_key hit (settle).
  // A full rebuild passes false so every slice misses and pixels stay put.
  bool rebuild_layout(const CameraKey& cam, const ContentFingerprint& fp,
                      bool reuse_slices);
  void clear_hillshade_bake();
  void absorb_layer_slices(const vista::MapIR& frame);

  // Mutex first: keeps offsetof stable across MapIR / vector ABI skew
  // between incremental objs (resource_deadlock_would_occur on bind).
  mutable std::recursive_mutex mu_;

  const GisScene* scene_ = nullptr;
  const ViewFrame* frame_ = nullptr;

  vista::MapIR cached_frame_;
  // Owned DrawItem copies keyed by cache_key. Not pointers into cached_frame_.
  std::unordered_map<uint64_t, std::vector<vista::DrawItem>> layer_slices_;
  ContentFingerprint cached_fp_;
  CameraKey cached_cam_;
  bool has_frame_cache_ = false;
  bool last_present_was_interactive_ = false;
  bool last_present_reused_layout_ = false;
  uint64_t layout_build_count_ = 0;
  // Bumped on bind/invalidate/rebuild so in-flight Layout::build can see a
  // newer gen than layout_gen and the host can skip publish.
  std::atomic<uint64_t> live_layout_gen_{0};
  // Last interactive present time (steady_clock). Settle waits ~200ms quiet.
  std::chrono::steady_clock::time_point last_interactive_tp_{};
  bool pending_interactive_clock_refresh_ = false;
  // Set when first china layout deferred DEM bake; next prepare rebuilds
  // with reuse_slices so hillshade_ms is not lumped into cold tess.
  bool pending_hillshade_attach_ = false;

  // Hillshade RGBA is a heap vector installed only after Layout::build
  // when live gen still matches. Stale gen keeps the published bake.
  static constexpr uint32_t kHillshadeTextureKey = 0x48534844u;  // 'HSHD'
  bool hillshade_ready_ = false;
  int hillshade_w_ = 0;
  int hillshade_h_ = 0;
  vista::TileSlot hillshade_slot_{};
  std::vector<uint8_t> hillshade_rgba_;

  base::Arena layout_scratch_{base::MemoryResource::Type::kMonotonicBuffer,
                              1 << 20};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_FRAME_CACHE_H_
