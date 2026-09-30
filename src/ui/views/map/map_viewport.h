// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_H_
#define UI_VIEWS_MAP_VIEWPORT_H_

#include "ui/ui_export.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "base/time/frame_timer.h"
#include "ui/gfx/raster/shell_raster.h"
#include "ui/gfx/display/vblank_wait.h"
#include "ui/views/map/touch_multitouch.h"
#include "ui/views/kernel/view/view.h"

namespace content {
class MapContents;
class ViewHost;
}

namespace ui {
namespace views {

// Native-hosted map pane. Hang is mgis-like: parent HWND + child HWND.
// Pixels still come from existing gis + render, or the same PE with --type=gpu.
//
// FlyCube present (P4/P5): a process-local Display mailbox thread exclusively
// owns rhi::Device initialize/resize/present/destroy. WM_PAINT does not call
// GPU present. BeginFrame is paced by display_cv_ wait (~16ms) with DXGI
// WaitForVBlank phase-align (ui::gfx::VblankClock).
// Twin API for OutputSurface draw_and_swap: gpu::PresentMailbox.
class UI_EXPORT MapViewport : public View {
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

  // Prefer content::MapWidgetHostView (OpenView kind from Role). Scene3d and
  // Map Edit/Data try FlyCube / present_gpu first by default.
  // Scene3d engine: content::set_scene3d_engine (View menu); non-FlyCube skips
  // FlyCube attach. SMT_FORCE_CONTENT_MAPVIEW_2D=1 / SMT_PREFER_FLYCUBE_2D=0 →
  // 2D ContentMapView. SMT_PREFER_GDI_DEVICE=1 skips FlyCube.
  // SMT_FORCE_GDI_MAP_OVERLAY=1 skips 2D gpu_present_ (caller uses full GDI
  // MapScene::paint).
  bool attach();
  AttachMode attach_mode() const { return mode_; }
  // Last FlyCube present result (updated on the Display mailbox thread).
  bool last_gpu_present_ok() const {
    return last_gpu_present_ok_.load(std::memory_order_acquire);
  }
  // Last ContentMapView SharedSurface blit into the paint DC (UI thread).
  // When true, overlay must not full-GDI the map (SoT already has vectors).
  bool last_content_present_ok() const {
    return last_content_present_ok_.load(std::memory_order_acquire);
  }
  const wchar_t* status_text() const { return status_; }
  bool wait_ready(uint32_t timeout_ms);

  void detach();

  // Optional shell overlay after GPU/present (layer vectors, selection).
  using OverlayPaint = std::function<void(HDC hdc, const RECT& client)>;
  void set_overlay_paint(OverlayPaint fn);
  // FlyCube present callback. Runs on the Display mailbox thread (P4), never
  // synchronously from WM_PAINT. Signature: (rhi::Device*, w, h) → ok.
  using GpuPresentFn =
      std::function<bool(void* rhi_device, uint32_t width_px, uint32_t height_px)>;
  void set_gpu_present(GpuPresentFn fn);
  // Optional enqueue hook for UI agents that own Commit themselves. When set,
  // BeginFrame calls this instead of GpuPresentFn. Runs on the Display thread.
  // UI agent may ignore GpuPresentFn and push CompositorFrame pieces here.
  using GpuSubmitFn = std::function<bool(void* rhi_device, uint32_t width_px,
                                         uint32_t height_px,
                                         uint32_t frame_token)>;
  void set_gpu_submit(GpuSubmitFn fn);
  // Stage pane-sized shell overlay (DrawRequest.shell). Generation skips
  // full re-copy when unchanged. When pixels actually change, requests a map
  // frame so FlyCube present can fold HUD in-GPU. Callers must filter
  // chrome-only dirty (BrowserView) so menu hover does not wake maps.
  // |hole_clear| pixels (ARGB) matching the shell clear under native map holes
  // get alpha forced to 0 so opaque chrome clear does not src-over the GPU map.
  void commit_shell_overlay(const uint8_t* bgra, uint32_t width_px,
                            uint32_t height_px, uint32_t stride_bytes,
                            uint64_t generation,
                            uint32_t hole_clear_argb = 0,
                            uint32_t hole_clear_argb_alt = 0);
  // HWND title + on-client identity HUD (engine name + FPS, legacy-style).
  void sync_identity_chrome();
  // Sample present cadence into hud_fps_ (Display or UI thread).
  void note_hud_frame();
  uint64_t shell_overlay_generation() const {
    return shell_generation_.load(std::memory_order_acquire);
  }
  // Copy staged shell for GpuPresentFn / DrawRequest.shell. Empty when none.
  bool snapshot_shell_overlay(std::vector<uint8_t>* out_bgra,
                              ui::gfx::ShellRaster* out_shell,
                              uint64_t* out_generation) const;
  void* rhi_device() const { return rhi_device_; }
  void invalidate_native();
  // Show/hide the owned DXGI present popup with the embed pane (tab switch).
  // Inactive Map-Edit present must not cover Scene3d.
  void set_flycube_present_visible(bool show);

  // Write the current backbuffer. False when no pixels have been presented.
  bool export_bmp(const std::string& path) const;

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  HWND create_native_view(HWND parent) override;
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  enum class DisplayOp {
    kInit,
    kResize,
    kDestroy,
  };

  struct DisplayTask {
    DisplayOp op = DisplayOp::kInit;
    HWND hwnd = nullptr;
    uint32_t width_px = 0;
    uint32_t height_px = 0;
  };

  bool try_content_map_view();
  bool try_oop_render();
  bool try_flycube_device();
  bool try_local_device();
  void paint_child_placeholder();
  // Blit MapWidgetHostView::Latest() (software DIB) into |hdc|. Shell owns
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

  void ensure_display_thread();
  void stop_display_thread();
  void shutdown_rhi_on_display_thread();
  void enqueue_display_task(DisplayTask task);
  void display_thread_main();
  void display_run_begin_frame();
  void display_run_present(uint32_t width_px, uint32_t height_px,
                           uint32_t frame_token);
  void request_frame();
  void signal_display();

  // Top-level DXGI present HWND for FlyCube (flip-model is unreliable on
  // WS_CHILD embed panes). Sized/moved over native_view().
  HWND ensure_flycube_present_hwnd(uint32_t width_px, uint32_t height_px);
  void sync_flycube_present_hwnd(uint32_t width_px, uint32_t height_px);
  void destroy_flycube_present_hwnd();

  static LRESULT CALLBACK child_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam);
  static LRESULT CALLBACK present_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
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
  // After init, only the Display mailbox thread may call Device methods.
  void* rhi_device_ = nullptr;
  uint32_t view_id_ = 0;
  uint32_t painted_generation_ = 0;
  // FlyCube: invalidate only while a frame was requested and not yet presented.
  std::atomic<uint32_t> frame_request_{1};
  std::atomic<uint32_t> frame_presented_{0};
  OverlayPaint overlay_paint_;
  GpuPresentFn gpu_present_;
  GpuSubmitFn gpu_submit_;
  std::atomic<bool> last_gpu_present_ok_{false};
  std::atomic<bool> last_content_present_ok_{false};
  bool frame_ready_ = false;
  HDC back_dc_ = nullptr;
  HBITMAP back_dib_ = nullptr;
  HBITMAP back_old_ = nullptr;
  int back_w_ = 0;
  int back_h_ = 0;
  // WM_POINTER touch contacts → midpoint InputEvent (pointer_count >= 2).
  TouchMultitouchTracker touch_tracker_;

  // On-client identity HUD (black bar + yellow engine/FPS), child of map or
  // FlyCube present HWND so DXGI flip surfaces still show it.
  HWND identity_badge_ = nullptr;
  HWND identity_badge_parent_ = nullptr;
  wchar_t identity_hud_text_[220] = {};
  mutable std::mutex hud_fps_mu_;
  base::FrameTimer hud_fps_timer_;
  std::atomic<float> hud_fps_{0.f};
  // Owned top-level FlyCube present surface (Scene3d / Map2d DXGI).
  HWND flycube_present_hwnd_ = nullptr;

  // Display / present mailbox (P4) + DWM/vblank BeginFrame (P5).
  // kPending while kInit runs. Queue-empty is not failure: the task is popped
  // before DX12 initialize returns.
  enum class DisplayInit : int { kIdle = 0, kPending = 1, kOk = 2, kFail = 3 };

  std::mutex display_mu_;
  std::condition_variable display_cv_;
  std::thread display_thread_;
  bool display_stop_ = false;
  bool display_started_ = false;
  bool display_destroy_ack_ = false;
  DisplayInit display_init_ = DisplayInit::kIdle;
  std::deque<DisplayTask> display_queue_;
  uint32_t display_client_w_ = 0;
  uint32_t display_client_h_ = 0;
  HWND display_hwnd_ = nullptr;
  ui::gfx::VblankClock display_vblank_;
  // QPC at last BeginFrame tick (Display thread); for latency counters.
  std::uint64_t last_begin_frame_qpc_ = 0;
  // Pane-cropped shell overlay for DrawRequest.shell / FlyCube overlay.
  mutable std::mutex shell_mu_;
  std::vector<uint8_t> shell_bgra_;
  uint32_t shell_width_px_ = 0;
  uint32_t shell_height_px_ = 0;
  uint32_t shell_stride_bytes_ = 0;
  std::atomic<uint64_t> shell_generation_{0};
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_H_
