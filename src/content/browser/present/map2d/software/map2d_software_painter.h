// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_SOFTWARE_PAINTER_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_SOFTWARE_PAINTER_H_

#include <cstddef>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"

namespace content {

class Map2dFrameCache;
class MapScene;
class ViewFrame;

// GDI software paint path for 2D maps. The map body is MapIR + View
// through paint_map_frame_gdi. Labels are MapIR kText glyphs.
class Map2dSoftwarePainter {
 public:
  Map2dSoftwarePainter() = default;
  ~Map2dSoftwarePainter();

  Map2dSoftwarePainter(const Map2dSoftwarePainter&) = delete;
  Map2dSoftwarePainter& operator=(const Map2dSoftwarePainter&) = delete;

  void bind(const MapScene* scene, const ViewFrame* frame,
            Map2dFrameCache* cache);

  void paint(HDC hdc, int width_px, int height_px) const;
  void paint(HDC hdc, int width_px, int height_px, bool fill_background) const;
  void paint_annotation_overlay(HDC hdc, int width_px, int height_px) const;
  void paint_flash_overlay(HDC hdc, int width_px, int height_px) const;

  bool export_bmp(const std::string& path, int width_px, int height_px) const;
  size_t basemap_tiles_drawn() const { return basemap_tiles_drawn_; }

  // Drop pixel reuse cache (call with Map2dFrameCache::invalidate).
  void invalidate_present_cache() const { clear_present_cache(); }

 private:
  void paint_basemap_underlay(HDC hdc, int width_px, int height_px) const;
  void paint_selection_overlay(HDC hdc, int width_px, int height_px) const;

  // Blit cached map DIB when layout+camera unchanged (StaticReuse / settle
  // debounce). Avoids full china GDI replay every InvalidateRect.
  bool try_blit_present_cache(HDC hdc, int width_px, int height_px,
                              uint64_t layout_gen) const;
  void store_present_cache(HDC src, int width_px, int height_px,
                           uint64_t layout_gen,
                           const Map2dFrameCache::CameraKey& cam) const;
  void clear_present_cache() const;

  const MapScene* scene_ = nullptr;
  const ViewFrame* frame_ = nullptr;
  Map2dFrameCache* cache_ = nullptr;
  mutable size_t basemap_tiles_drawn_ = 0;

  mutable HBITMAP present_cache_bmp_ = nullptr;
  mutable HDC present_cache_dc_ = nullptr;
  mutable int present_cache_w_ = 0;
  mutable int present_cache_h_ = 0;
  mutable uint64_t present_cache_layout_gen_ = 0;
  mutable Map2dFrameCache::CameraKey present_cache_cam_{};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_SOFTWARE_PAINTER_H_
