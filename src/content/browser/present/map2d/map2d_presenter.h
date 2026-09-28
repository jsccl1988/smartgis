// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_PRESENTER_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_PRESENTER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"
#include "content/browser/present/map2d/gpu/map2d_gpu_present.h"
#include "content/browser/present/map2d/software/map2d_software_painter.h"
#include "ui/gfx/raster/shell_raster.h"

namespace render {
namespace rhi {
class Device;
}
}  // namespace render

namespace content {

class MapScene;
class ViewFrame;

// Thin 2D present facade: shared Map2dFrameCache + GPU / software backends.
class Map2dPresenter {
 public:
  Map2dPresenter();
  ~Map2dPresenter();

  Map2dPresenter(const Map2dPresenter&) = delete;
  Map2dPresenter& operator=(const Map2dPresenter&) = delete;

  void bind(const MapScene* scene, const ViewFrame* frame);

  Map2dFrameCache& frame_cache() { return cache_; }
  const Map2dFrameCache& frame_cache() const { return cache_; }
  Map2dGpuPresent& gpu() { return gpu_; }
  const Map2dGpuPresent& gpu() const { return gpu_; }
  Map2dSoftwarePainter& software() { return software_; }
  const Map2dSoftwarePainter& software() const { return software_; }

  void invalidate_frame_cache();

  bool present_gpu(render::rhi::Device* device, uint32_t width_px,
                   uint32_t height_px,
                   const ui::gfx::ShellRaster* shell = nullptr,
                   uint64_t shell_generation = 0);
  bool last_gpu_present_ok() const;
  uint64_t layout_build_count() const;
  bool last_present_reused_layout() const;

  void paint(HDC hdc, int width_px, int height_px) const;
  void paint(HDC hdc, int width_px, int height_px, bool fill_background) const;
  void paint_annotation_overlay(HDC hdc, int width_px, int height_px) const;
  void paint_flash_overlay(HDC hdc, int width_px, int height_px) const;
  void paint_labels_projected(
      HDC hdc, int width_px, int height_px,
      const std::function<void(double lon, double lat, int* sx, int* sy)>&
          project) const;

  bool export_bmp(const std::string& path, int width_px, int height_px) const;
  size_t basemap_tiles_drawn() const;

 private:
  Map2dFrameCache cache_;
  Map2dGpuPresent gpu_;
  Map2dSoftwarePainter software_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_PRESENTER_H_
