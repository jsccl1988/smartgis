// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_SCENIC_SCENIC_RHI2D_HOST_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_SCENIC_SCENIC_RHI2D_HOST_H_

#include <memory>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "gis/map/map.h"

class GDALDataset;

namespace scenic {
class Engine;
}

namespace content {

class MapScene;
class ViewFrame;

namespace detail {

// True when a saved BMP is the Scenic ocean-clear key (no carto landed).
bool map2d_bmp_is_ocean_clear(const char* path);

// Hosts Scenic rhi2d (Renderer2d + scenic_rhi2d_{gdi,gdiplus,skia}) on a
// map HWND. Product SoT when --map2d-engine=scenic; gis::Map is the paint
// document. src/legacy is frozen and is not used here.
class ScenicRhi2dHost {
 public:
  ScenicRhi2dHost();
  ~ScenicRhi2dHost();

  ScenicRhi2dHost(const ScenicRhi2dHost&) = delete;
  ScenicRhi2dHost& operator=(const ScenicRhi2dHost&) = delete;

  bool attach(HWND hwnd, int width_px, int height_px);
  bool is_ready() const;
  bool last_present_ok() const { return last_ok_; }

  bool paint_hdc(HDC hdc, int width_px, int height_px, const ViewFrame* frame);
  bool export_bmp(const std::string& path, int width_px, int height_px,
                  const ViewFrame* frame);

  // Map2dEngine DrawItem path (--map2d-engine=scenic). Lives in this TU so
  // Map2dPresenter does not include scenic/engine.h.
  void ensure_draw_engine();
  void shutdown_draw_engine();
  bool hosts_draw_engine() const;
  bool draw_engine_last_ok() const;
  bool export_draw_engine_bmp(const MapScene* scene, const ViewFrame* frame,
                              const std::string& path, int width_px,
                              int height_px);

  void shutdown();

 private:

 private:
  bool ensure_map();
  bool ensure_device();
  bool apply_view(int width_px, int height_px, const ViewFrame* frame);
  bool refresh_frame(int width_px, int height_px, const ViewFrame* frame,
                     bool settle_frame_job);

  HWND hwnd_ = nullptr;
  int width_px_ = 0;
  int height_px_ = 0;
  bool last_ok_ = false;
  gis::Map map_;
  GDALDataset* dataset_ = nullptr;
  struct DeviceState;
  std::unique_ptr<DeviceState> device_;
  std::unique_ptr<scenic::Engine> draw_engine_;
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SCENIC_SCENIC_RHI2D_HOST_H_
