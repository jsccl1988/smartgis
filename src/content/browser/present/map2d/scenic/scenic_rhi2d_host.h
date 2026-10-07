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

class GisScene;
class ViewFrame;

namespace detail {

// True when a saved BMP is the Scenic ocean-clear key (no carto landed).
bool map2d_bmp_is_ocean_clear(const char* path);

// Opt-in Scenic rhi2d HWND host (--map2d-engine=scenic only).
// Product map2d SoT is Vista MapPass + Map2dFrameCache; this host must not
// construct devices or Map2dEngine on the default product path.
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
  // Map2dPresenter does not include scenic/engine.h. No-op when scenic is off.
  void ensure_draw_engine();
  void shutdown_draw_engine();
  bool hosts_draw_engine() const;
  bool draw_engine_last_ok() const;
  bool export_draw_engine_bmp(const GisScene* scene, const ViewFrame* frame,
                              const std::string& path, int width_px,
                              int height_px);

  void shutdown();

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
