// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/viewport/map_viewport.h"

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
#include "render/rhi/rhi.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/frame/identity_hud.h"
#include "ui/views/map/device/device_load.h"
#include "ui/views/map/frame/embed_fill.h"
#include "ui/views/map/viewport/features.h"
#include "ui/views/map/input/viewport_input.h"

namespace ui {
namespace views {

namespace {

const wchar_t kMapClass[] = L"SmartGisMapViewport";

}  // namespace

using detail::DeviceObj;
using detail::exe_dir;
using detail::file_exists;

MapViewport::MapViewport() {
  set_preferred_size({400, 300});
}

MapViewport::~MapViewport() {
  detach();
  stop_display_thread();
}

void MapViewport::set_role(Role role) {
  role_ = role;
}

void MapViewport::set_view_host(content::ViewHost* host) {
  view_host_ = host;
}

void MapViewport::set_map_contents(content::MapContents* session) {
  if (owns_session_ && session_ && session_ != session) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
    session_->Shutdown();
    delete session_;
#endif
  }
  session_ = session;
  owns_session_ = false;
}

HWND MapViewport::create_native_view(HWND parent) {
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MapViewport::child_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    // Opaque stock brush: NULL_BRUSH + WS_CLIPCHILDREN shows the desktop
    // through the map hole before the first WM_PAINT FillRect (startup hollow).
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kMapClass;
    registered = RegisterClassExW(&wc) != 0;
  }
  HWND hwnd = CreateWindowExW(0, kMapClass, L"",
                              WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                  WS_CLIPCHILDREN,
                              0, 0, 1, 1, parent, nullptr,
                              GetModuleHandleW(nullptr), this);
  return hwnd;
}

void MapViewport::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  // Native child paints the map. Shell draws a caption strip above if
  // the host left a gap; here the child fills the view.
}

bool MapViewport::attach() {
  const char* role_name =
      role_ == Role::kScene3d
          ? "scene3d"
          : (role_ == Role::kMapData ? "map_data" : "map_edit");
  if (!native_view()) {
    realize_native();
  }
  if (!native_view()) {
    mode_ = AttachMode::kPlaceholder;
    status_ = L"No map HWND";
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

  // Scene3d / Map 2D default: FlyCube RHI. Scene3d engine is a runtime
  // preference (View menu / content::set_scene3d_engine); 2D still allows
  // FORCE_CONTENT / PREFER_FLYCUBE env for ContentMapView. ContentMapView /
  // OOP / GDI remain fallbacks.
#if defined(SMT_HAS_SCENE3D_ENGINE)
  const bool prefer_flycube_3d = content::prefer_scene3d_flycube();
#else
  const bool prefer_flycube_3d = true;
#endif
  const bool prefer_flycube_2d = []() {
    // Scenic MemFrame present must never create a FlyCube HWND — residual
    // display_run_present SEH 0xC0000005 was observed when scenic still
    // attached DX12 present on the shell map panes.
    if (const char* map_eng = std::getenv("SMT_MAP2D_ENGINE")) {
      if (map_eng[0] && _stricmp(map_eng, "scenic") == 0) {
        return false;
      }
    }
    if (const char* scene_eng = std::getenv("SMT_SCENE3D_ENGINE")) {
      if (scene_eng[0] && _stricmp(scene_eng, "scenic") == 0) {
        return false;
      }
    }
    if (const char* env = std::getenv("SMT_FORCE_CONTENT_MAPVIEW_2D")) {
      if (env[0] == '1' && env[1] == '\0') {
        return false;
      }
    }
    if (const char* prefer = std::getenv("SMT_PREFER_FLYCUBE_2D")) {
      if (prefer[0] == '0' && prefer[1] == '\0') {
        return false;
      }
    }
    return true;
  }();
#ifdef SMT_HAS_FLYCUBE
  constexpr int k_has_flycube = 1;
#else
  constexpr int k_has_flycube = 0;
#endif
  LOGGING(LOG_INFO,
          "rhi.attach policy role=%s prefer_flycube_2d=%d prefer_flycube_3d=%d "
          "SMT_HAS_FLYCUBE=%d",
          role_name, prefer_flycube_2d ? 1 : 0, prefer_flycube_3d ? 1 : 0,
          k_has_flycube);
  if (role_ == Role::kScene3d && prefer_flycube_3d) {
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"3D FlyCube RHI present (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=FlyCube/DX12 ok", role_name);
      sync_identity_frame();
      return true;
    }
    LOGGING(LOG_WARNING, "rhi.attach role=%s FlyCube failed; trying fallbacks",
            role_name);
  }
  if ((role_ == Role::kMapEdit || role_ == Role::kMapData) && prefer_flycube_2d) {
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = (role_ == Role::kMapData) ? L"2D data FlyCube RHI (DX12)"
                                         : L"2D map FlyCube RHI (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=FlyCube/DX12 ok", role_name);
      sync_identity_frame();
      return true;
    }
    LOGGING(LOG_WARNING, "rhi.attach role=%s FlyCube failed; trying fallbacks",
            role_name);
  }

  if (try_content_map_view()) {
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
  // Map Edit fallbacks: OOP / FlyCube / LoadLibrary. Scene3d: FlyCube again.
  if (role_ == Role::kMapEdit) {
    if (try_oop_render()) {
      mode_ = AttachMode::kOopRender;
      status_ = L"OOP SmartGisRender.exe";
      paint_child_placeholder();
      LOGGING(LOG_WARNING, "rhi.attach role=%s mode=OOP", role_name);
      sync_identity_frame();
      return true;
    }
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"2D map FlyCube RHI (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=FlyCube/DX12 ok (retry)",
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
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"2D data FlyCube RHI (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=FlyCube/DX12 ok (retry)",
              role_name);
      sync_identity_frame();
      return true;
    }
  } else if (role_ == Role::kScene3d) {
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"3D FlyCube RHI present (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=FlyCube/DX12 ok (retry)",
              role_name);
      sync_identity_frame();
      return true;
    }
  }
  mode_ = AttachMode::kPlaceholder;
  if (role_ == Role::kScene3d) {
    status_ = L"3D placeholder (no scene device)";
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

void MapViewport::detach() {
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
#ifdef SMT_HAS_CONTENT_MAP_SESSION
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
  destroy_flycube_present_hwnd();
  view_id_ = 0;
  mode_ = AttachMode::kNone;
  last_gpu_present_ok_.store(false, std::memory_order_release);
  last_content_present_ok_.store(false, std::memory_order_release);
}

void MapViewport::refresh_has_gpu_cb() {
  const bool has =
      std::atomic_load_explicit(&gpu_present_, std::memory_order_acquire) !=
          nullptr ||
      std::atomic_load_explicit(&gpu_submit_, std::memory_order_acquire) !=
          nullptr;
  has_gpu_cb_.store(has, std::memory_order_release);
}

void MapViewport::set_overlay_paint(OverlayPaint fn) {
  std::shared_ptr<OverlayPaint> next;
  if (fn) {
    next = std::make_shared<OverlayPaint>(std::move(fn));
  }
  std::atomic_store_explicit(&overlay_paint_, std::move(next),
                             std::memory_order_release);
}

void MapViewport::set_gpu_present(GpuPresentFn fn) {
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

void MapViewport::set_gpu_submit(GpuSubmitFn fn) {
  std::shared_ptr<GpuSubmitFn> next;
  if (fn) {
    next = std::make_shared<GpuSubmitFn>(std::move(fn));
  }
  std::atomic_store_explicit(&gpu_submit_, std::move(next),
                             std::memory_order_release);
  refresh_has_gpu_cb();
  request_frame();
}

void MapViewport::request_frame() {
  frame_request_.fetch_add(1, std::memory_order_acq_rel);
  signal_display();
  // FlyCube: Display mailbox owns DXGI present. InvalidateRect only wakes
  // ContentMapView / GDI overlay paths and burned UI-thread paint for free.
  if (mode_ == AttachMode::kFlyCube) {
    return;
  }
  if (HWND hwnd = native_view()) {
    InvalidateRect(hwnd, nullptr, FALSE);
  }
}

uint32_t MapViewport::frame_request() const {
  return frame_request_.load(std::memory_order_acquire);
}

uint32_t MapViewport::frame_presented() const {
  return frame_presented_.load(std::memory_order_acquire);
}

void MapViewport::mark_gpu_surface_dirty() {
  gpu_surface_dirty_.store(true, std::memory_order_release);
}

bool MapViewport::consume_gpu_surface_dirty() {
  return gpu_surface_dirty_.exchange(false, std::memory_order_acq_rel);
}

void MapViewport::invalidate_native() {
  request_frame();
}

bool MapViewport::wait_ready(uint32_t timeout_ms) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
  if (session_ && view_id_ != 0) {
    return session_->WaitFrameReady(view_id_, timeout_ms);
  }
#endif
  return native_view() && IsWindow(native_view());
}

void MapViewport::resize_host_surface(int width_px, int height_px) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
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

float MapViewport::surface_dpi() const {
  if (widget()) {
    return static_cast<float>(widget()->dpi());
  }
  return static_cast<float>(dpi_for_hwnd(native_view()));
}

void MapViewport::on_device_scale_factor_changed(float old_scale,
                                               float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  if (HWND hwnd = native_view()) {
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    resize_host_surface(rc.right - rc.left, rc.bottom - rc.top);
  }
}

bool MapViewport::try_content_map_view() {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
  if (!session_) {
    session_ = content::MapContents::Create();
    if (!session_) {
      return false;
    }
    owns_session_ = true;
  }
  // Shared Browser MapSession may own MapContents without StartRenderProcess
  // (deferred OOP). Start now — first true need for the pipe.
  if (!session_->IsOopRender()) {
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

bool MapViewport::try_oop_render() {
#ifdef SMT_HAS_UI_SHELL
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
