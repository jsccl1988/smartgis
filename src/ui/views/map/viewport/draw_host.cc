// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/viewport/draw_host.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

#include "base/core/log.h"
#include "base/process/switches.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/frame/identity_hud.h"
#include "ui/views/map/device/device_load.h"
#include "ui/views/map/frame/embed_fill.h"
#include "ui/views/map/viewport/features.h"
#include "ui/views/map/viewport/paint_policy.h"
#include "ui/views/map/input/viewport_input.h"

namespace ui {
namespace views {

namespace {

const wchar_t kDrawHostClass[] = L"SmartGisDrawHost";

}  // namespace

using detail::DeviceObj;
using detail::exe_dir;
using detail::file_exists;

DrawHost::DrawHost() {
  set_preferred_size({400, 300});
}

DrawHost::~DrawHost() {
  detach();
  stop_display_thread();
}

void DrawHost::set_role(Role role) {
  role_ = role;
}

void DrawHost::set_view_host(content::ViewHost* host) {
  view_host_ = host;
}

void DrawHost::set_map_contents(content::MapContents* session) {
  if (owns_session_ && session_ && session_ != session) {
#ifdef HAS_CONTENT_MAP_SESSION
    session_->Shutdown();
    delete session_;
#endif
  }
  session_ = session;
  owns_session_ = false;
}

HWND DrawHost::create_native_view(HWND parent) {
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DrawHost::child_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    // Opaque stock brush: NULL_BRUSH + WS_CLIPCHILDREN shows the desktop
    // through the embed hole before the first WM_PAINT FillRect (startup hollow).
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kDrawHostClass;
    registered = RegisterClassExW(&wc) != 0;
  }
  HWND hwnd = CreateWindowExW(0, kDrawHostClass, L"",
                              WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                  WS_CLIPCHILDREN,
                              0, 0, 1, 1, parent, nullptr,
                              GetModuleHandleW(nullptr), this);
  return hwnd;
}

void DrawHost::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  // Native child paints the map. Shell draws a caption strip above if
  // the host left a gap; here the child fills the view.
}

bool DrawHost::attach() {
  const char* role_name =
      role_ == Role::kScene3d
          ? "scene3d"
          : (role_ == Role::kMapData ? "map_data" : "map_edit");
  if (!native_view()) {
    realize_native();
  }
  if (!native_view()) {
    mode_ = AttachMode::kPlaceholder;
    status_ = L"No host HWND";
    LOGGING(LOG_ERROR, "rhi.attach role=%s fail: no HWND", role_name);
    return false;
  }
  SetWindowLongPtrW(native_view(), GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(this));

  RECT rc0 = {};
  GetClientRect(native_view(), &rc0);
  LOGGING(LOG_INFO, "rhi.attach begin role=%s hwnd=%p client=%dx%d", role_name,
          native_view(), static_cast<int>(rc0.right),
          static_cast<int>(rc0.bottom));

  // Scene3d / 2D default: GPU RHI. Scene3d engine is a runtime
  // preference (View menu / content::set_scene3d_engine); 2D still allows
  // FORCE_CONTENT / PREFER_FLYCUBE env for ContentMapView. ContentMapView /
  // OOP / GDI remain fallbacks. Scenic keeps the product HWND presenter
  // (Scene3dPresenter::paint) — never ContentMapView leftover SharedSurface.
#if defined(HAS_SCENE3D_ENGINE)
  const bool prefer_rhi_3d = content::prefer_scene3d_flycube();
  const bool scenic_3d = content::prefer_scene3d_scenic();
  const bool gdi_3d = content::prefer_scene3d_gdi();
  const bool force_content_3d = content::force_content_mapview_3d();
#else
  const bool prefer_rhi_3d = true;
  const bool scenic_3d = false;
  const bool gdi_3d = false;
  const bool force_content_3d = false;
#endif
  const bool prefer_gpu_present_2d = []() {
    // Scenic MemFrame present must never create a GPU present HWND — residual
    // display_run_present SEH 0xC0000005 was observed when scenic still
    // attached DX12 present on the shell draw panes.
    if (detail::prefer_map2d_scenic_engine()) {
      return false;
    }
    if (const char* scene_eng = base::switch_cstr("scene3d-engine")) {
      if (scene_eng[0] && _stricmp(scene_eng, "scenic") == 0) {
        return false;
      }
    }
    if (detail::force_content_mapview_2d()) {
      return false;
    }
    if (const char* prefer = base::switch_cstr("prefer-flycube-2d")) {
      if (prefer[0] == '0' && prefer[1] == '\0') {
        return false;
      }
    }
    return true;
  }();
#ifdef HAS_FLYCUBE
  constexpr int k_has_gpu_rhi = 1;
#else
  constexpr int k_has_gpu_rhi = 0;
#endif
  LOGGING(LOG_INFO,
          "rhi.attach policy role=%s prefer_gpu_present_2d=%d prefer_rhi_3d=%d "
          "scenic_3d=%d force_content_3d=%d HAS_FLYCUBE=%d",
          role_name, prefer_gpu_present_2d ? 1 : 0, prefer_rhi_3d ? 1 : 0,
          scenic_3d ? 1 : 0, force_content_3d ? 1 : 0, k_has_gpu_rhi);
  // Scenic / GDI Scene3d: product HWND + Scene3dPresenter (MemFrame /
  // paint_hdc). ContentMapView SharedSurface is not a 3D SoT and the
  // on-screen navy fill + overlay flash every present tick.
  if (role_ == Role::kScene3d && (scenic_3d || gdi_3d)) {
    mode_ = AttachMode::kPlaceholder;
    status_ = scenic_3d ? L"Scene3dPresenter (scenic)"
                        : L"Scene3dPresenter (GDI)";
    start_present_timer();
    LOGGING(LOG_INFO, "rhi.attach role=%s mode=%s/product HWND", role_name,
            scenic_3d ? "Scenic" : "GDI");
    sync_identity_frame();
    return true;
  }
  if (role_ == Role::kScene3d && prefer_rhi_3d) {
    if (try_gpu_present_device()) {
      mode_ = AttachMode::kGpuPresent;
      status_ = L"3D GPU present (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=GpuPresent/DX12 ok", role_name);
      sync_identity_frame();
      return true;
    }
    LOGGING(LOG_WARNING, "rhi.attach role=%s GPU present failed; trying fallbacks",
            role_name);
  }
  if ((role_ == Role::kMapEdit || role_ == Role::kMapData) && prefer_gpu_present_2d) {
    if (try_gpu_present_device()) {
      mode_ = AttachMode::kGpuPresent;
      status_ = (role_ == Role::kMapData) ? L"2D data GPU present (DX12)"
                                         : L"2D map GPU present (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=GpuPresent/DX12 ok", role_name);
      sync_identity_frame();
      return true;
    }
    LOGGING(LOG_WARNING, "rhi.attach role=%s GPU present failed; trying fallbacks",
            role_name);
  }
  // Scenic 2D: product HWND + Map2dPresenter, same as scenic Scene3d.
  // ContentMapView SharedSurface is leftover GPU demo (orange tessellation).
  if ((role_ == Role::kMapEdit || role_ == Role::kMapData) &&
      detail::prefer_map2d_scenic_engine() &&
      !detail::force_content_mapview_2d()) {
    mode_ = AttachMode::kPlaceholder;
    status_ = L"Map2dPresenter (scenic)";
    start_present_timer();
    LOGGING(LOG_INFO, "rhi.attach role=%s mode=Scenic/product HWND", role_name);
    sync_identity_frame();
    return true;
  }

  // Scene3d ContentMapView is stereo-only. GPU present failure falls through to
  // Scene3dPresenter on the product HWND — not a second SharedSurface SoT.
  const bool allow_content_3d = role_ != Role::kScene3d || force_content_3d;
  if (allow_content_3d && try_content_map_view()) {
    mode_ = AttachMode::kContentMapView;
    status_ = (role_ == Role::kScene3d)
                  ? L"content::MapWidgetHostView (3D SoT)"
                  : L"content::MapWidgetHostView";
    start_present_timer();
    paint_child_placeholder();
    LOGGING(LOG_WARNING, "rhi.attach role=%s mode=ContentMapView (fallback)",
            role_name);
    sync_identity_frame();
    return true;
  }
  // Map Edit fallbacks: OOP / GPU present / LoadLibrary. Scene3d: GPU present again.
  if (role_ == Role::kMapEdit) {
    if (try_oop_render()) {
      mode_ = AttachMode::kOopRender;
      status_ = L"OOP SmartGisRender.exe";
      paint_child_placeholder();
      LOGGING(LOG_WARNING, "rhi.attach role=%s mode=OOP", role_name);
      sync_identity_frame();
      return true;
    }
    if (try_gpu_present_device()) {
      mode_ = AttachMode::kGpuPresent;
      status_ = L"2D map GPU present (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=GpuPresent/DX12 ok (retry)",
              role_name);
      sync_identity_frame();
      return true;
    }
    if (try_local_device()) {
      mode_ = AttachMode::kLocalDevice;
      status_ = L"CreateRenderDevice (LoadLibrary)";
      LOGGING(LOG_WARNING, "rhi.attach role=%s mode=LocalDevice", role_name);
      sync_identity_frame();
      return true;
    }
  } else if (role_ == Role::kMapData) {
    if (try_gpu_present_device()) {
      mode_ = AttachMode::kGpuPresent;
      status_ = L"2D data GPU present (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=GpuPresent/DX12 ok (retry)",
              role_name);
      sync_identity_frame();
      return true;
    }
  } else if (role_ == Role::kScene3d) {
    if (try_gpu_present_device()) {
      mode_ = AttachMode::kGpuPresent;
      status_ = L"3D GPU present (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=GpuPresent/DX12 ok (retry)",
              role_name);
      sync_identity_frame();
      return true;
    }
  }
  mode_ = AttachMode::kPlaceholder;
  if (role_ == Role::kScene3d) {
    // Software / scenic-style DEM via Scene3dPresenter::paint overlay.
    status_ = L"3D placeholder (Scene3dPresenter software)";
    start_present_timer();
  } else if (role_ == Role::kMapData) {
    status_ = L"Datasource browse (placeholder)";
  } else {
    status_ = L"Placeholder map (no render exe / device DLL)";
  }
  paint_child_placeholder();
  LOGGING(LOG_ERROR, "rhi.attach role=%s mode=Placeholder (all backends failed)",
          role_name);
  // HWND is live; callers treat placeholder as a successful UI hang.
  sync_identity_frame();
  return native_view() != nullptr;
}

void DrawHost::detach() {
  stop_present_timer();
  release_backbuffer();
  touch_tracker_.clear();
  if (identity_badge_) {
    if (IsWindow(identity_badge_)) {
      DestroyWindow(identity_badge_);
    }
    identity_badge_ = nullptr;
    identity_badge_parent_ = nullptr;
  }
#ifdef HAS_CONTENT_MAP_SESSION
  if (owns_session_ && session_) {
    session_->Shutdown();
    delete session_;
  }
#endif
  session_ = nullptr;
  owns_session_ = false;
  painted_generation_ = 0;
  if (HWND hwnd = native_view()) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
  }
  if (render_process_) {
    TerminateProcess(render_process_, 0);
    CloseHandle(render_process_);
    render_process_ = nullptr;
  }
  if (render_job_) {
    CloseHandle(render_job_);
    render_job_ = nullptr;
  }
  if (local_device_) {
    auto* obj = static_cast<DeviceObj*>(local_device_);
    if (obj->vtbl && obj->vtbl->Release) {
      obj->vtbl->Release(obj);
    }
    local_device_ = nullptr;
  }
  local_module_ = nullptr;
  // Stop the mailbox before (or while) waiting on kDestroy so close cannot
  // sit forever if Display was between frames. shutdown_rhi still runs on
  // thread exit when destroy is skipped.
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    if (display_started_) {
      display_stop_ = true;
    }
  }
  display_cv_.notify_all();
  // Drain Display mailbox and destroy Device before HWND teardown.
  release_rhi_device();
  stop_display_thread();
  // Drop callbacks only after the mailbox thread has joined.
  std::atomic_store_explicit(&overlay_paint_, std::shared_ptr<OverlayPaint>{},
                             std::memory_order_release);
  std::atomic_store_explicit(&gpu_present_, std::shared_ptr<GpuPresentFn>{},
                             std::memory_order_release);
  std::atomic_store_explicit(&gpu_submit_, std::shared_ptr<GpuSubmitFn>{},
                             std::memory_order_release);
  has_gpu_cb_.store(false, std::memory_order_release);
  destroy_gpu_present_hwnd();
  view_id_ = 0;
  mode_ = AttachMode::kNone;
  last_gpu_present_ok_.store(false, std::memory_order_release);
  last_content_present_ok_.store(false, std::memory_order_release);
}

void DrawHost::refresh_has_gpu_cb() {
  const bool has =
      std::atomic_load_explicit(&gpu_present_, std::memory_order_acquire) !=
          nullptr ||
      std::atomic_load_explicit(&gpu_submit_, std::memory_order_acquire) !=
          nullptr;
  has_gpu_cb_.store(has, std::memory_order_release);
}

void DrawHost::set_overlay_paint(OverlayPaint fn) {
  std::shared_ptr<OverlayPaint> next;
  if (fn) {
    next = std::make_shared<OverlayPaint>(std::move(fn));
  }
  std::atomic_store_explicit(&overlay_paint_, std::move(next),
                             std::memory_order_release);
}

void DrawHost::set_gpu_present(GpuPresentFn fn) {
  // Callback runs on the Display mailbox thread (P4), not on WM_PAINT.
  std::shared_ptr<GpuPresentFn> next;
  if (fn) {
    next = std::make_shared<GpuPresentFn>(std::move(fn));
  }
  std::atomic_store_explicit(&gpu_present_, std::move(next),
                             std::memory_order_release);
  refresh_has_gpu_cb();
  request_frame();
}

void DrawHost::set_gpu_submit(GpuSubmitFn fn) {
  std::shared_ptr<GpuSubmitFn> next;
  if (fn) {
    next = std::make_shared<GpuSubmitFn>(std::move(fn));
  }
  std::atomic_store_explicit(&gpu_submit_, std::move(next),
                             std::memory_order_release);
  refresh_has_gpu_cb();
  request_frame();
}

void DrawHost::request_frame() {
  frame_request_.fetch_add(1, std::memory_order_acq_rel);
  signal_display();
  // GPU present: Display mailbox owns DXGI present. InvalidateRect only wakes
  // ContentMapView / GDI overlay paths and burned UI-thread paint for free.
  if (mode_ == AttachMode::kGpuPresent) {
    return;
  }
  if (HWND hwnd = native_view()) {
    InvalidateRect(hwnd, nullptr, FALSE);
  }
}

uint32_t DrawHost::frame_request() const {
  return frame_request_.load(std::memory_order_acquire);
}

uint32_t DrawHost::frame_presented() const {
  return frame_presented_.load(std::memory_order_acquire);
}

void DrawHost::mark_gpu_surface_dirty() {
  gpu_surface_dirty_.store(true, std::memory_order_release);
}

bool DrawHost::consume_gpu_surface_dirty() {
  return gpu_surface_dirty_.exchange(false, std::memory_order_acq_rel);
}

void DrawHost::invalidate_native() {
  request_frame();
}

bool DrawHost::wait_ready(uint32_t timeout_ms) {
#ifdef HAS_CONTENT_MAP_SESSION
  if (session_ && view_id_ != 0) {
    return session_->WaitFrameReady(view_id_, timeout_ms);
  }
#endif
  return native_view() && IsWindow(native_view());
}

void DrawHost::resize_host_surface(int width_px, int height_px) {
#ifdef HAS_CONTENT_MAP_SESSION
  if (!session_ || view_id_ == 0 || width_px <= 0 || height_px <= 0) {
    return;
  }
  if (content::MapWidgetHostView* view = session_->HostView(view_id_)) {
    view->Resize(width_px, height_px, surface_dpi());
  }
#else
  (void)width_px;
  (void)height_px;
#endif
}

float DrawHost::surface_dpi() const {
  if (widget()) {
    return static_cast<float>(widget()->dpi());
  }
  return static_cast<float>(dpi_for_hwnd(native_view()));
}

void DrawHost::on_device_scale_factor_changed(float old_scale,
                                               float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  if (HWND hwnd = native_view()) {
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    resize_host_surface(rc.right - rc.left, rc.bottom - rc.top);
  }
}

bool DrawHost::try_content_map_view() {
#ifdef HAS_CONTENT_MAP_SESSION
  if (!session_) {
    session_ = content::MapContents::Create();
    if (!session_) {
      return false;
    }
    owns_session_ = true;
  }
  // Shared BrowserSession may own MapContents without StartRenderProcess
  // (deferred until ensure_oop_render_process / ENABLE_OOP_RENDER).
  // DISABLE_OOP_RENDER skips the GPU child (HelloWait ~15s on cold start).
  // force-content-mapview-2d only selects ContentMapView / software DIB attach;
  // WaitFrameReady still needs kFrameReady from the GPU pipe, so do not treat
  // that switch as in-process-only.
  if (!session_->IsOopRender()) {
    if (!base::switch_is_one("disable-oop-render")) {
      if (!session_->StartRenderProcess()) {
        if (owns_session_) {
          session_->Shutdown();
          delete session_;
          session_ = nullptr;
          owns_session_ = false;
        }
        return false;
      }
    }
  }
  content::ViewKind kind = content::ViewKind::kMapEdit;
  if (role_ == Role::kMapData) {
    kind = content::ViewKind::kMapData;
  } else if (role_ == Role::kScene3d) {
    kind = content::ViewKind::kScene3d;
  }
  view_id_ = session_->OpenView(kind);
  if (view_id_ == 0) {
    return false;
  }
  // Software DIB: GPU publishes shared pixels; this HWND presents Latest().
  // kChildHwnd is reserved for a future in-GPU child window; both fall back
  // to create_dib in PresentTarget::resize today.
  if (content::MapWidgetHostView* view = session_->AttachSurface(
          view_id_, content::PresentMode::kSoftwareDib)) {
    content::MapWidgetHostView::CreateParams params;
    params.parent_hwnd = native_view();
    view->Create(params, content::MapWidgetHostView::Preferences{});
    RECT rc = {};
    GetClientRect(native_view(), &rc);
    const int w = rc.right > 0 ? rc.right : 64;
    const int h = rc.bottom > 0 ? rc.bottom : 64;
    view->Resize(w, h, surface_dpi());
  }
  return true;
#else
  return false;
#endif
}

bool DrawHost::try_oop_render() {
#ifdef HAS_UI_SHELL
  if (ui::shell::IMapSession* session = ui::shell::create_map_session()) {
    if (session->start_render_process()) {
      session->open_view(ui::shell::ViewKind::kMapEdit);
      session->attach_surface(1, ui::shell::PresentMode::kChildHwnd);
      return true;
    }
  }
#endif

  wchar_t dir[MAX_PATH] = {};
  exe_dir(dir, MAX_PATH);
  wchar_t path[MAX_PATH] = {};
  lstrcpynW(path, dir, MAX_PATH);
  lstrcpynW(path + lstrlenW(path), L"SmartGisRender.exe",
            MAX_PATH - lstrlenW(path));
  if (!file_exists(path)) {
    return false;
  }

  const DWORD pid = GetCurrentProcessId();
  wchar_t session[64] = {};
  swprintf_s(session, L"%u-%lu", pid, GetTickCount());

  wchar_t cmd[1024] = {};
  swprintf_s(cmd,
             L"\"%s\" --parent-pid=%u --pipe=smartgis-host-%u --session=%s "
             L"--map-hwnd=%llu --present=child_hwnd",
             path, pid, pid, session,
             static_cast<unsigned long long>(
                 reinterpret_cast<uintptr_t>(native_view())));

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessW(path, cmd, nullptr, nullptr, FALSE, CREATE_SUSPENDED,
                      nullptr, dir, &si, &pi)) {
    return false;
  }

  render_job_ = CreateJobObjectW(nullptr, nullptr);
  if (render_job_) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim = {};
    lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(render_job_, JobObjectExtendedLimitInformation,
                            &lim, sizeof(lim));
    AssignProcessToJobObject(render_job_, pi.hProcess);
  }
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  render_process_ = pi.hProcess;
  return true;
}

}  // namespace views
}  // namespace ui
