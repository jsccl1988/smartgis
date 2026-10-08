// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_DRAW_HOST_H_
#define UI_VIEWS_MAP_DRAW_HOST_H_

#include "ui/ui_export.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
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
#include "ui/views/map/input/touch_multitouch.h"
#include "ui/views/kernel/view/view.h"

namespace content {
class GisContents;
class ToolSession;
}

namespace ui {
namespace views {

// Primary draw surface of the Views shell (HWND canvas). Hang is mgis-like:
// parent HWND + child HWND. Pixels come from content/render below UI, or
// the same PE with --type=gpu. This type is not a GIS "map viewport".
//
// GPU present (P4/P5): a process-local Display mailbox thread exclusively
// owns rhi::Device initialize/resize/present/destroy. WM_PAINT does not call
// GPU present. BeginFrame is paced by display_cv_ wait (~16ms) with DXGI
// WaitForVBlank phase-align (ui::gfx::VblankClock).
// Twin API for OutputSurface draw_and_swap: gpu::PresentMailbox.
class UI_EXPORT DrawHost : public View {
 public:
  enum class AttachMode {
    kNone,
    kContentMapView,
    kOopRender,
    kGpuPresent,
    kLocalDevice,
    kPlaceholder,
  };

  // Which GisContents::OpenView kind this pane should request.
  enum class Role {
    kMapEdit,
    kMapData,
    kScene3d,
  };

  DrawHost();
  ~DrawHost() override;

  void set_role(Role role);
  Role role() const { return role_; }

  void set_tool_session(content::ToolSession* host);
  content::ToolSession* tool_session() const { return tool_session_; }

  // Non-owning shared session. When unset, attach() may create one.
  void set_gis_contents(content::GisContents* session);
  content::GisContents* gis_contents() const { return session_; }

  uint32_t view_id() const { return view_id_; }

  // Prefer content::WidgetHostView (OpenView kind from Role). Scene3d and
  // Map Edit/Data try GPU present / present_gpu first by default.
  // Scene3d engine: content::set_scene3d_engine (View menu); Scenic and GDI
  // keep the product HWND + Scene3dPresenter (no ContentMapView). Stereo may
  // force ContentMapView. FORCE_CONTENT_MAPVIEW_2D=1 / PREFER_FLYCUBE_2D=0 鈫?  // 2D ContentMapView. PREFER_GDI_DEVICE=1 skips GPU present.
  // FORCE_GDI_MAP_OVERLAY=1 skips 2D gpu_present_ (caller uses full GDI
  // GisScene::paint).
  bool attach();
  AttachMode attach_mode() const { return mode_; }
  // Last GPU present result (updated on the Display mailbox thread).
  bool last_gpu_present_ok() const {
    return last_gpu_present_ok_.load(std::memory_order_acquire);
  }
  // BeginFrame mailbox tokens 鈥?wait for presented >= request after invalidate.
  uint32_t frame_request() const;
  uint32_t frame_presented() const;
  // DXGI Resize/initialize cleared the swapchain 鈥?next present must redraw.
  void mark_gpu_surface_dirty();
  bool consume_gpu_surface_dirty();
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
  // GPU present callback. Runs on the Display mailbox thread (P4), never
  // synchronously from WM_PAINT. Signature: (rhi::Device*, w, h) 鈫?ok.
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
  // frame so GPU present can fold HUD in-GPU. Callers must filter
  // horizon-only dirty (BrowserView) so menu hover does not wake maps.
  // |hole_clear| pixels (ARGB) matching the shell clear under native map holes
  // get alpha forced to 0 so opaque horizon clear does not src-over the GPU map.
  void commit_shell_overlay(const uint8_t* bgra, uint32_t width_px,
                            uint32_t height_px, uint32_t stride_bytes,
                            uint64_t generation,
                            uint32_t hole_clear_argb = 0,
                            uint32_t hole_clear_argb_alt = 0);
  // Drop staged shell overlay (resize / move). Stale pane-sized BGRA can
  // ghost Feature/Identify horizon onto the map until the next shell publish.
  void clear_shell_overlay();
  // HWND title + on-client identity HUD (engine name + FPS, legacy-style).
  void sync_identity_frame();
  // Throttled (~250ms) sync_identity_frame after note_hud_frame.
  void maybe_sync_identity_hud();
  // Sample present cadence into hud_fps_ (Display or UI thread).
  void note_hud_frame();
  // Smoothed present FPS. First/idle samples cap dt at 250ms so HUD is not
  // stuck at Fps0.000 after attach.
  float hud_fps() const {
    return hud_fps_.load(std::memory_order_relaxed);
  }
  uint64_t shell_overlay_generation() const {
    return shell_generation_.load(std::memory_order_acquire);
  }
  // Copy staged shell for GpuPresentFn / DrawRequest.shell. Empty when none.
  bool snapshot_shell_overlay(std::vector<uint8_t>* out_bgra,
                              ui::gfx::ShellRaster* out_shell,
                              uint64_t* out_generation) const;
  void* rhi_device() const { return rhi_device_; }
  void invalidate_native();
  // Wake the Display mailbox / present timer (shell China seed, tab switch).
  void request_frame();
  // HWND that receives user mouse (DXGI GPU present popup when visible, else embed).
  // Gesture subclass + shell wheel forward must target this, not native_view()
  // alone 鈥?the present popup sits above the embed and steals hit-testing.
  HWND input_hwnd() const;
  // Owned DXGI GPU present popup, or null. Visible or not 鈥?capture must
  // BitBlt this HWND (WS_EX_NOREDIRECTIONBITMAP), not the navy embed hole.
  HWND present_hwnd() const;
  // Layered identity HUD popup (GPU present HWND) or child HWND (GDI embed).
  HWND identity_hud_hwnd() const;
  // Show/hide the owned DXGI present popup with the embed pane (tab switch).
  // Inactive Map-Edit present must not cover Scene3d.
  void set_gpu_present_visible(bool show);
  // Hide the DXGI popup and KillTimer(kPresentTimerId). Queued present ticks
  // are ignored via present_paused_ (do not PeekMessage 鈥?that processes
  // sent Display messages and deadlocks UI鈫擠isplay on tab switch).
  void pause_present();
  // Restart the 16ms present WM_TIMER after harness stop_map_present_timers
  // or pause_present() so the HWND is not stuck on a single ocean clear.
  void resume_present_timer();

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
  bool try_gpu_present_device();
  bool try_local_device();
  void paint_child_placeholder();
  // Blit WidgetHostView::Latest() for Scene3d ContentMapView only.
  // 2D leftover GPU DIBs are a demo tessellation 鈥?Map2dPresenter overlay
  // is the 2D SoT (Vista GDI / Scenic MemFrame).
  bool present_latest_frame(HDC hdc, const RECT& client_rc);
  void start_present_timer();
  void stop_present_timer();
  void resize_host_surface(int width_px, int height_px);
  float surface_dpi() const;
  void release_rhi_device();
  // Offscreen DIB used so present + overlay land as one BitBlt (no flicker).
  bool ensure_backbuffer(int width_px, int height_px);
  void release_backbuffer();
  void paint_host_content(HDC target, const RECT& client_rc);

  void handle_present_timer(HWND hwnd);
  LRESULT handle_paint(HWND hwnd);
  void handle_size(HWND hwnd, int cx, int cy);
  void handle_embed_move(HWND hwnd);
  // True when the message is fully consumed (horizon retarget).
  bool handle_mouse_capture(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  void publish_display_client_size(uint32_t width_px, uint32_t height_px);

  void ensure_display_thread();
  void stop_display_thread();
  void shutdown_rhi_on_display_thread();
  void enqueue_display_task(DisplayTask task);
  void display_thread_main();
  void display_run_begin_frame();
  void display_run_present(uint32_t width_px, uint32_t height_px,
                           uint32_t frame_token);
  void signal_display();

  // Top-level DXGI present HWND for GPU present (flip-model is unreliable on
  // WS_CHILD embed panes). Sized/moved over native_view().
  HWND ensure_gpu_present_hwnd(uint32_t width_px, uint32_t height_px);
  void sync_gpu_present_hwnd(uint32_t width_px, uint32_t height_px);
  void destroy_gpu_present_hwnd();
  // Show the DXGI popup only after Init has cleared the NOREDIRECTION hole.
  void reveal_gpu_present_if_ready();

  static LRESULT CALLBACK child_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam);
  static LRESULT CALLBACK present_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                           LPARAM lparam);

  static constexpr UINT_PTR kPresentTimerId = 1;
  // Display thread posts this to the embed HWND; reveal must run on the UI
  // thread that owns the DXGI present popup (cross-thread SetWindowPos hangs).
  static constexpr UINT kMsgRevealGpuPresent = WM_APP + 0x5256;  // 'RV'

  AttachMode mode_ = AttachMode::kNone;
  Role role_ = Role::kMapEdit;
  const wchar_t* status_ = L"";
  content::GisContents* session_ = nullptr;
  content::ToolSession* tool_session_ = nullptr;
  bool owns_session_ = false;
  HANDLE render_process_ = nullptr;
  HANDLE render_job_ = nullptr;
  void* local_device_ = nullptr;
  HMODULE local_module_ = nullptr;
  // Owning render::rhi::Device* when AttachMode::kGpuPresent.
  // After init, only the Display mailbox thread may call Device methods.
  void* rhi_device_ = nullptr;
  uint32_t view_id_ = 0;
  uint32_t painted_generation_ = 0;
  // GPU present: invalidate only while a frame was requested and not yet presented.
  std::atomic<uint32_t> frame_request_{1};
  std::atomic<uint32_t> frame_presented_{0};
  // Publish via std::atomic_store; readers std::atomic_load. Hot path only
  // bumps a shared_ptr refcount 鈥?never copies std::function under a mutex.
  // (Unsynchronized std::function assign + call AVs in _Tidy.)
  void refresh_has_gpu_cb();
  std::shared_ptr<OverlayPaint> overlay_paint_;
  std::shared_ptr<GpuPresentFn> gpu_present_;
  std::shared_ptr<GpuSubmitFn> gpu_submit_;
  std::atomic<bool> has_gpu_cb_{false};
  std::atomic<bool> last_gpu_present_ok_{false};
  std::atomic<bool> last_content_present_ok_{false};
  // Set on Display Resize/Init after swapchain recreate; consumed by present.
  std::atomic<bool> gpu_surface_dirty_{false};
  bool frame_ready_ = false;
  HDC back_dc_ = nullptr;
  HBITMAP back_dib_ = nullptr;
  HBITMAP back_old_ = nullptr;
  int back_w_ = 0;
  int back_h_ = 0;
  // WM_POINTER touch contacts 鈫?midpoint InputEvent (pointer_count >= 2).
  TouchMultitouchTracker touch_tracker_;

  // On-client identity HUD (black bar + yellow engine/FPS). Child of the
  // embed, or a layered popup owned by the GPU present present HWND (GDI children
  // of WS_EX_NOREDIRECTIONBITMAP DXGI surfaces do not paint).
  HWND identity_badge_ = nullptr;
  HWND identity_badge_parent_ = nullptr;
  wchar_t identity_hud_text_[220] = {};
  mutable std::mutex hud_fps_mu_;
  base::FrameTimer hud_fps_timer_;
  std::atomic<float> hud_fps_{0.f};
  // Throttle identity HUD text / geometry sync (WM_TIMER ~16 ms).
  DWORD last_hud_sync_tick_ = 0;
  // Owned top-level GPU present present surface (Scene3d / Map2d DXGI).
  HWND gpu_present_hwnd_ = nullptr;
  // Tab wants the DXGI popup visible; stay hidden until Init has Present'd
  // once (WS_EX_NOREDIRECTIONBITMAP is a desktop hole before that).
  std::atomic<bool> gpu_present_want_visible_{false};
  std::atomic<bool> present_paused_{false};
  // Coalesce Display→UI reveal posts (BeginFrame can fire faster than UI).
  std::atomic<bool> reveal_posted_{false};

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
  // Pane-cropped shell overlay for DrawRequest.shell / GPU overlay.
  mutable std::mutex shell_mu_;
  std::vector<uint8_t> shell_bgra_;
  uint32_t shell_width_px_ = 0;
  uint32_t shell_height_px_ = 0;
  uint32_t shell_stride_bytes_ = 0;
  std::atomic<uint64_t> shell_generation_{0};
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_DRAW_HOST_H_
