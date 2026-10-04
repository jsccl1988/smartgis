// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_FRAME_CACHE_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_FRAME_CACHE_H_

#include <cstdint>
#include <chrono>
#include <mutex>
#include <vector>

#include "base/memory/arena.h"
#include "vista/frame/frame.h"

namespace content {

class MapScene;
class ViewFrame;

// Shared MapFrame cache for GPU present and GDI paint (dual-speed §Perf).
class Map2dFrameCache {
 public:
  Map2dFrameCache() = default;

  Map2dFrameCache(const Map2dFrameCache&) = delete;
  Map2dFrameCache& operator=(const Map2dFrameCache&) = delete;

  void bind(const MapScene* scene, const ViewFrame* frame);
  void invalidate();

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
  const vista::MapFrame& frame() const { return cached_frame_; }
  const CameraKey& camera() const { return cached_cam_; }
  uint64_t layout_build_count() const { return layout_build_count_; }
  bool last_present_reused_layout() const {
    return last_present_reused_layout_;
  }

  // Host-baked DEM hillshade RGBA for Pass / GDI textured raster DrawItems.
  // Soft-fails (returns false) when Style has no hillshade or DEM is missing.
  bool load_raster(uint32_t texture_key, std::vector<uint8_t>* rgba, int* w,
                   int* h) const;
  bool has_hillshade_underlay() const { return hillshade_ready_; }

  // Recursive: GPU present holds this across prepare + Pass record.
  std::recursive_mutex& mutex() { return mu_; }

 private:
  ContentFingerprint make_fingerprint() const;
  CameraKey make_camera_key(uint32_t width_px, uint32_t height_px) const;
  bool rebuild_layout(const CameraKey& cam, const ContentFingerprint& fp);
  void clear_hillshade_bake();

  // Mutex first: keeps offsetof stable across MapFrame / vector ABI skew
  // between incremental objs (resource_deadlock_would_occur on bind).
  mutable std::recursive_mutex mu_;

  const MapScene* scene_ = nullptr;
  const ViewFrame* frame_ = nullptr;

  vista::MapFrame cached_frame_;
  ContentFingerprint cached_fp_;
  CameraKey cached_cam_;
  bool has_frame_cache_ = false;
  bool last_present_was_interactive_ = false;
  bool last_present_reused_layout_ = false;
  uint64_t layout_build_count_ = 0;
  // Last interactive present time (steady_clock). Settle waits ~200ms quiet.
  std::chrono::steady_clock::time_point last_interactive_tp_{};
  bool pending_interactive_clock_refresh_ = false;

  // Hillshade RGBA is a heap vector installed only after Layout::build.
  // rebuild_layout must clear prior frame/hillshade before Arena/TLS reset.
  static constexpr uint32_t kHillshadeTextureKey = 0x48534844u;  // 'HSHD'
  bool hillshade_ready_ = false;
  int hillshade_w_ = 0;
  int hillshade_h_ = 0;
  std::vector<uint8_t> hillshade_rgba_;

  base::Arena layout_scratch_{base::MemoryResource::Type::kMonotonicBuffer,
                              1 << 20};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_FRAME_CACHE_H_
