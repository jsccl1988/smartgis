// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_PRESENT_MAP2D_PRESENTER_H_
#define APP_VIEWS_PRESENT_MAP2D_PRESENTER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "app/views/present/host/shell_overlay_effect.h"
#include "gis/vista/frame/frame.h"
#include "ui/gfx/raster/shell_raster.h"

namespace effect {
namespace map {
class Pass;
}
}  // namespace effect

namespace render {
namespace rhi {
class Device;
}
}  // namespace render

namespace app {

class MapScene;
class ViewFrame;

// Thin 2D present facade (Chromium WebContents-ish): bind scene/frame, own
// GPU pass lifetime, dual-speed MapFrame cache, and dispatch GDI paint /
// export. Frame math and carto live in map2d/frame/; GDI stroke work in
// map2d/paint/.
class Map2dPresenter {
 public:
  Map2dPresenter();
  ~Map2dPresenter();

  Map2dPresenter(const Map2dPresenter&) = delete;
  Map2dPresenter& operator=(const Map2dPresenter&) = delete;

  void bind(const MapScene* scene, const ViewFrame* frame);

  // Drop cached MapFrame so the next present_gpu rebuilds layout.
  void invalidate_frame_cache();

  // |shell| / |shell_generation| mirror gpu::DrawRequest.shell (src-over HUD).
  // Null / empty shell skips the overlay quad. Generation skip avoids re-upload.
  bool present_gpu(render::rhi::Device* device, uint32_t width_px,
                   uint32_t height_px,
                   const ui::gfx::ShellRaster* shell = nullptr,
                   uint64_t shell_generation = 0);
  bool last_gpu_present_ok() const { return last_gpu_present_ok_; }

  // How many times Layout::build ran since bind / invalidate. Tests use this
  // to assert interactive/static reuse.
  uint64_t layout_build_count() const { return layout_build_count_; }
  bool last_present_reused_layout() const { return last_present_reused_layout_; }

  void paint(HDC hdc, int width_px, int height_px) const;
  void paint(HDC hdc, int width_px, int height_px, bool fill_background) const;
  void paint_annotation_overlay(HDC hdc, int width_px, int height_px) const;
  void paint_flash_overlay(HDC hdc, int width_px, int height_px) const;
  void paint_labels_projected(
      HDC hdc, int width_px, int height_px,
      const std::function<void(double lon, double lat, int* sx, int* sy)>&
          project) const;

  bool export_bmp(const std::string& path, int width_px, int height_px) const;
  size_t basemap_tiles_drawn() const { return basemap_tiles_drawn_; }

 private:
  struct ContentFingerprint {
    size_t feature_count = 0;
    size_t layer_count = 0;
    const void* style_ptr = nullptr;
    bool use_carto = true;

    bool operator==(const ContentFingerprint& o) const {
      return feature_count == o.feature_count && layer_count == o.layer_count &&
             style_ptr == o.style_ptr && use_carto == o.use_carto;
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

  ContentFingerprint make_fingerprint() const;
  CameraKey make_camera_key(uint32_t width_px, uint32_t height_px) const;
  bool rebuild_layout(const CameraKey& cam, const ContentFingerprint& fp);
  bool present_frame(render::rhi::Device* device, const CameraKey& cam,
                     bool record_all, const ui::gfx::ShellRaster* shell,
                     uint64_t shell_generation);

  void paint_basemap_underlay(HDC hdc, int width_px, int height_px) const;
  void paint_selection_overlay(HDC hdc, int width_px, int height_px) const;

  const MapScene* scene_ = nullptr;
  const ViewFrame* frame_ = nullptr;
  std::unique_ptr<effect::map::Pass> map2d_pass_;
  detail::ShellOverlayEffect shell_overlay_;
  bool last_gpu_present_ok_ = false;
  mutable size_t basemap_tiles_drawn_ = 0;

  gis::vista::MapFrame cached_frame_;
  ContentFingerprint cached_fp_;
  CameraKey cached_cam_;
  bool has_frame_cache_ = false;
  bool last_present_was_interactive_ = false;
  bool last_present_reused_layout_ = false;
  uint64_t layout_build_count_ = 0;
};

}  // namespace app

#endif  // APP_VIEWS_PRESENT_MAP2D_PRESENTER_H_
