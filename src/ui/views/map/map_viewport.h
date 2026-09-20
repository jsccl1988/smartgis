// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_H_
#define UI_VIEWS_MAP_VIEWPORT_H_

#include <cstdint>
#include <functional>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/map/touch_multitouch.h"
#include "ui/views/kernel/view.h"

namespace content {
class MapContents;
class ViewHost;
}

namespace ui {
namespace views {

// Native-hosted map pane. Hang is mgis-like: parent HWND + child HWND.
// Pixels still come from existing gis + render, or the same PE with --type=gpu.
class MapViewport : public View {
 public:
  enum class AttachMode {
    kNone,
    kContentMapView,
    kOopRender,
    kFlyCube,
    kLocalDevice,
    kPlaceholder,
  };

  // Which MapContents::OpenView kind this pane should request.
  enum class Role {
    kMapEdit,
    kMapData,
    kScene3d,
  };

  MapViewport();
  ~MapViewport() override;

  void set_role(Role role);
  Role role() const { return role_; }

  void set_view_host(content::ViewHost* host);
  content::ViewHost* view_host() const { return view_host_; }

  // Non-owning shared session. When unset, attach() may create one.
  void set_map_contents(content::MapContents* session);
  content::MapContents* map_contents() const { return session_; }

  uint32_t view_id() const { return view_id_; }

  // Prefer content::MapWidgetHostView (OpenView kind from Role). Scene3d tries
  // FlyCube / present_gpu first by default; set SMT_FORCE_CONTENT_MAPVIEW_3D=1
  // Default Scene3d: ContentMapView SoT stereo. Opt in FlyCube with
  // SMT_PREFER_FLYCUBE_3D=1 (or FORCE_CONTENT=1 to force ContentMapView). Map Edit:
  // OOP / FlyCube / LoadLibrary. SMT_PREFER_GDI_DEVICE=1 skips FlyCube.
  bool attach();
  AttachMode attach_mode() const { return mode_; }
  // Last Scene3d FlyCube gpu_present_ result (false until a successful present).
  bool last_gpu_present_ok() const { return last_gpu_present_ok_; }
  const wchar_t* status_text() const { return status_; }
  bool wait_ready(uint32_t timeout_ms);

  void detach();

  // Optional chrome overlay after GPU/present (layer vectors, selection).
  using OverlayPaint = std::function<void(HDC hdc, const RECT& client)>;
  void set_overlay_paint(OverlayPaint fn);
  // Scene3d + FlyCube: record/draw/present using the hung RHI device.
  // Signature: (rhi::Device*, width_px, height_px) → present ok.
  using GpuPresentFn =
      std::function<bool(void* rhi_device, uint32_t width_px, uint32_t height_px)>;
  void set_gpu_present(GpuPresentFn fn);
  void* rhi_device() const { return rhi_device_; }
  void invalidate_native();

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  HWND create_native_view(HWND parent) override;
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  bool try_content_map_view();
  bool try_oop_render();
  bool try_flycube_device();
  bool try_local_device();
  void paint_child_placeholder();
  // Blit MapWidgetHostView::Latest() (software DIB) into |hdc|. Chrome owns
  // the HWND; GPU only publishes shared pixels (see map_widget_host_view.h).
  bool present_latest_frame(HDC hdc, const RECT& client_rc);
  void start_present_timer();
  void stop_present_timer();
  void resize_host_surface(int width_px, int height_px);
  float surface_dpi() const;
  void release_rhi_device();
  // Offscreen DIB used so present + overlay land as one BitBlt (no flicker).
  bool ensure_backbuffer(int width_px, int height_px);
  void release_backbuffer();
  void paint_map_content(HDC target, const RECT& client_rc);

  static LRESULT CALLBACK child_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam);

  static constexpr UINT_PTR kPresentTimerId = 1;

  AttachMode mode_ = AttachMode::kNone;
  Role role_ = Role::kMapEdit;
  const wchar_t* status_ = L"";
  content::MapContents* session_ = nullptr;
  content::ViewHost* view_host_ = nullptr;
  bool owns_session_ = false;
  HANDLE render_process_ = nullptr;
  HANDLE render_job_ = nullptr;
  void* local_device_ = nullptr;
  HMODULE local_module_ = nullptr;
  // Owning render::rhi::Device* when AttachMode::kFlyCube.
  void* rhi_device_ = nullptr;
  uint32_t view_id_ = 0;
  uint32_t painted_generation_ = 0;
  OverlayPaint overlay_paint_;
  GpuPresentFn gpu_present_;
  bool last_gpu_present_ok_ = false;
  HDC back_dc_ = nullptr;
  HBITMAP back_dib_ = nullptr;
  HBITMAP back_old_ = nullptr;
  int back_w_ = 0;
  int back_h_ = 0;
  // WM_POINTER touch contacts → midpoint InputEvent (pointer_count >= 2).
  TouchMultitouchTracker touch_tracker_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_H_
