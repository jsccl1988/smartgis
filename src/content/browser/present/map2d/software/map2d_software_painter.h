// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_SOFTWARE_PAINTER_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_SOFTWARE_PAINTER_H_

#include <cstddef>
#include <functional>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {

class Map2dFrameCache;
class MapScene;
class ViewFrame;

// GDI software paint path for 2D maps. Full map paint rasters shared MapFrame;
// overlays / basemap / projected labels stay Scene-driven.
class Map2dSoftwarePainter {
 public:
  Map2dSoftwarePainter() = default;

  Map2dSoftwarePainter(const Map2dSoftwarePainter&) = delete;
  Map2dSoftwarePainter& operator=(const Map2dSoftwarePainter&) = delete;

  void bind(const MapScene* scene, const ViewFrame* frame,
            Map2dFrameCache* cache);

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
  void paint_basemap_underlay(HDC hdc, int width_px, int height_px) const;
  void paint_selection_overlay(HDC hdc, int width_px, int height_px) const;

  const MapScene* scene_ = nullptr;
  const ViewFrame* frame_ = nullptr;
  Map2dFrameCache* cache_ = nullptr;
  mutable size_t basemap_tiles_drawn_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_SOFTWARE_PAINTER_H_
