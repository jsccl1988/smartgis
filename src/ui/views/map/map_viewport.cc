// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/map_viewport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
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

#if defined(__has_include)
#if __has_include("content/public/map_contents.h")
#include "content/public/map_contents.h"
#define SMT_HAS_CONTENT_MAP_SESSION 1
#endif
#if __has_include("content/public/view_host.h")
#include "content/public/view_host.h"
#define SMT_HAS_VIEW_HOST 1
#endif
#if __has_include("content/browser/present/scene3d/policy/scene3d_rhi_session.h")
#include "content/browser/present/scene3d/policy/scene3d_rhi_session.h"
#define SMT_HAS_SCENE3D_ENGINE 1
#endif
#if __has_include("ui/shell/map_session.h")
#include "ui/shell/map_session.h"
#define SMT_HAS_UI_SHELL 1
#endif
#endif

namespace ui {
namespace views {
namespace {

const wchar_t kMapClass[] = L"SmartGisMapViewport";
// Top-level FlyCube DXGI target (flip-model fails on WS_CHILD embed panes).
const wchar_t kFlyCubePresentClass[] = L"SmartGisFlyCubePresent";
// Legacy-matching top HUD: black bar + yellow engine name + Fps.
const wchar_t kIdentityHudClass[] = L"SmartGisMapIdentityHud";
constexpr int kIdentityHudHeight = 28;

using CreateRenderDeviceFn = int (*)(HINSTANCE, void*&);

struct DeviceVtable {
  void* dtor;
  int (*Init)(void* self, HWND hwnd, const char* logname);
  int (*Destroy)(void* self);
  int (*Release)(void* self);
  int (*Resize)(void* self, int orgx, int orgy, int cx, int cy);
};

struct DeviceObj {
  DeviceVtable* vtbl;
};

LRESULT CALLBACK identity_hud_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                       LPARAM lparam) {
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    const wchar_t* text =
        reinterpret_cast<const wchar_t*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (text && text[0]) {
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, RGB(255, 255, 0));
      TextOutW(hdc, 8, 6, text, lstrlenW(text));
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  if (msg == WM_NCHITTEST) {
    // Map / orbit input hits the parent under the HUD.
    return HTTRANSPARENT;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void register_identity_hud_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = identity_hud_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kIdentityHudClass;
  RegisterClassExW(&wc);
  done = true;
}

bool file_exists(const wchar_t* path) {
  const DWORD attr = GetFileAttributesW(path);
  return attr != INVALID_FILE_ATTRIBUTES &&
         !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Optional Scene3d swapchain downscale. A buffer smaller than the HWND client
// (e.g. forced 640x480 on a ~2k child) clears navy on-screen even when
// DrawIndexed succeeds — DXGI does not stretch that path the way a matching
// showcase HWND does. Default: native client size. Opt in:
// SMT_SCENE3D_SWAPCHAIN_MAX=1280x720 (or WIDTHxHEIGHT).
void clamp_scene3d_swapchain_size(uint32_t* w, uint32_t* h) {
  if (!w || !h) {
    return;
  }
  // Showcase-sized present: interactive multi-k clients clear navy with
  // DrawIndexed ok; 640x480 top-level matches the land-PASS atmosphere shot.
  if (const char* force = std::getenv("SMT_SCENE3D_FORCE_640")) {
    if (force[0] == '1' && force[1] == '\0') {
      *w = 640;
      *h = 480;
      return;
    }
  }
  const char* spec = std::getenv("SMT_SCENE3D_SWAPCHAIN_MAX");
  if (!spec || !spec[0]) {
    return;
  }
  unsigned max_w = 0;
  unsigned max_h = 0;
  if (std::sscanf(spec, "%ux%u", &max_w, &max_h) != 2 || max_w < 64u ||
      max_h < 64u) {
    return;
  }
  if (*w <= max_w && *h <= max_h) {
    return;
  }
  const float scale =
      (std::min)(static_cast<float>(max_w) / static_cast<float>(*w),
                 static_cast<float>(max_h) / static_cast<float>(*h));
  *w = (std::max)(64u, static_cast<uint32_t>(*w * scale));
  *h = (std::max)(64u, static_cast<uint32_t>(*h * scale));
}

void exe_dir(wchar_t* out, size_t cap) {
  GetModuleFileNameW(nullptr, out, static_cast<DWORD>(cap));
  wchar_t* slash = wcsrchr(out, L'\\');
  if (slash) {
    slash[1] = 0;
  }
}

HMODULE load_first(const wchar_t* const* names) {
  for (size_t i = 0; names[i]; ++i) {
    if (HMODULE already = GetModuleHandleW(names[i])) {
      return already;
    }
    if (HMODULE mod = LoadLibraryW(names[i])) {
      return mod;
    }
  }
  return nullptr;
}

bool init_device_seh(void* device, HWND hwnd) {
  auto* obj = static_cast<DeviceObj*>(device);
  if (!obj || !obj->vtbl || !obj->vtbl->Init) {
    return false;
  }
  __try {
    return obj->vtbl->Init(obj, hwnd, "SmartGisViews") == 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

// Same Win32 → InputEvent mapping as leftover dispatch_shell_message for
// mouse / wheel / key. Two-finger pan: WM_POINTER* → TouchMultitouchTracker
// (midpoint, pointer_count >= 2), matching CEF shell.js.
bool route_view_host_input(content::ViewHost* host,
                           HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam,
                           bool suppress_mouse) {
#ifdef SMT_HAS_VIEW_HOST
  if (!host) {
    return false;
  }
  if (suppress_mouse) {
    switch (message) {
      case WM_MOUSEMOVE:
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_LBUTTONDBLCLK:
      case WM_RBUTTONDOWN:
      case WM_RBUTTONUP:
      case WM_RBUTTONDBLCLK:
      case WM_MBUTTONDOWN:
      case WM_MBUTTONUP:
        return true;  // swallow primary-contact mouse synthesis
      default:
        break;
    }
  }
  content::InputEvent e{};
  switch (message) {
    case WM_MOUSEMOVE:
      e.kind = content::InputEvent::Kind::kMouseMove;
      break;
    case WM_LBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kLDown;
      break;
    case WM_LBUTTONUP:
      e.kind = content::InputEvent::Kind::kLUp;
      break;
    case WM_LBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kLDClick;
      break;
    case WM_RBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kRDown;
      break;
    case WM_RBUTTONUP:
      e.kind = content::InputEvent::Kind::kRUp;
      break;
    case WM_RBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kRDClick;
      break;
    case WM_MBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kRDown;
      e.flags = MK_MBUTTON;
      break;
    case WM_MBUTTONUP:
      e.kind = content::InputEvent::Kind::kRUp;
      e.flags = MK_MBUTTON;
      break;
    case WM_MOUSEWHEEL: {
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = static_cast<int32_t>(GET_WHEEL_DELTA_WPARAM(wparam));
      e.flags = static_cast<uint32_t>(GET_KEYSTATE_WPARAM(wparam));
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      if (hwnd) {
        ScreenToClient(hwnd, &pt);
      }
      e.x_px = pt.x;
      e.y_px = pt.y;
      if (e.flags & MK_SHIFT) {
        e.wheel = -e.wheel;
      }
      return host->dispatch_input(e);
    }
    case WM_MOUSEHWHEEL: {
      // Trackpad / mouse horizontal wheel → pan (see input_flags::kHorizontalWheel).
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = static_cast<int32_t>(GET_WHEEL_DELTA_WPARAM(wparam));
      e.flags = static_cast<uint32_t>(GET_KEYSTATE_WPARAM(wparam)) |
                content::input_flags::kHorizontalWheel;
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      if (hwnd) {
        ScreenToClient(hwnd, &pt);
      }
      e.x_px = pt.x;
      e.y_px = pt.y;
      return host->dispatch_input(e);
    }
    case WM_KEYDOWN:
      e.kind = content::InputEvent::Kind::kKeyDown;
      e.key = static_cast<uint32_t>(wparam);
      return host->dispatch_input(e);
    default:
      return false;
  }
  e.x_px = static_cast<int32_t>(static_cast<short>(LOWORD(lparam)));
  e.y_px = static_cast<int32_t>(static_cast<short>(HIWORD(lparam)));
  e.flags = static_cast<uint32_t>(wparam);
  return host->dispatch_input(e);
#else
  (void)host;
  (void)hwnd;
  (void)message;
  (void)wparam;
  (void)lparam;
  (void)suppress_mouse;
  return false;
#endif
}

// Maps WM_POINTER* touch contacts into multitouch InputEvents.
bool route_view_host_pointer(content::ViewHost* host,
                             HWND hwnd,
                             UINT message,
                             WPARAM wparam,
                             TouchMultitouchTracker* tracker) {
#ifdef SMT_HAS_VIEW_HOST
  if (!host || !tracker || !hwnd) {
    return false;
  }
  switch (message) {
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
    case WM_POINTERCAPTURECHANGED:
      break;
    default:
      return false;
  }

  const UINT32 pointer_id = GET_POINTERID_WPARAM(wparam);
  POINTER_INPUT_TYPE pointer_type = PT_POINTER;
  if (!GetPointerType(pointer_id, &pointer_type) ||
      pointer_type != PT_TOUCH) {
    return false;
  }

  POINTER_INFO info = {};
  if (!GetPointerInfo(pointer_id, &info)) {
    return false;
  }
  POINT pt = info.ptPixelLocation;
  ScreenToClient(hwnd, &pt);
  const int32_t x_px = pt.x;
  const int32_t y_px = pt.y;

  content::InputEvent e{};
  bool have_sample = false;
  if (message == WM_POINTERDOWN) {
    have_sample = tracker->on_contact_down(pointer_id, x_px, y_px, &e);
  } else if (message == WM_POINTERUPDATE) {
    have_sample = tracker->on_contact_move(pointer_id, x_px, y_px, &e);
  } else {
    // UP / CAPTURECHANGED: drop the contact (lost capture counts as lift).
    have_sample = tracker->on_contact_up(pointer_id, x_px, y_px, &e);
  }
  if (!have_sample) {
    // Multitouch session: swallow remaining pointer noise so DefWindowProc
    // does not synthesize mouse. Single-finger: fall through for mouse.
    return tracker->suppress_mouse();
  }
  return host->dispatch_input(e);
#else
  (void)host;
  (void)hwnd;
  (void)message;
  (void)wparam;
  (void)tracker;
  return false;
#endif
}

}  // namespace

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
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
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
      sync_identity_chrome();
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
      sync_identity_chrome();
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
    sync_identity_chrome();
    return true;
  }
  // Map Edit fallbacks: OOP / FlyCube / LoadLibrary. Scene3d: FlyCube again.
  if (role_ == Role::kMapEdit) {
    if (try_oop_render()) {
      mode_ = AttachMode::kOopRender;
      status_ = L"OOP SmartGisRender.exe";
      paint_child_placeholder();
      LOGGING(LOG_WARNING, "rhi.attach role=%s mode=OOP", role_name);
      sync_identity_chrome();
      return true;
    }
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"2D map FlyCube RHI (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=FlyCube/DX12 ok (retry)",
              role_name);
      sync_identity_chrome();
      return true;
    }
    if (try_local_device()) {
      mode_ = AttachMode::kLocalDevice;
      status_ = L"CreateRenderDevice (LoadLibrary)";
      LOGGING(LOG_WARNING, "rhi.attach role=%s mode=LocalDevice", role_name);
      sync_identity_chrome();
      return true;
    }
  } else if (role_ == Role::kMapData) {
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"2D data FlyCube RHI (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=FlyCube/DX12 ok (retry)",
              role_name);
      sync_identity_chrome();
      return true;
    }
  } else if (role_ == Role::kScene3d) {
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"3D FlyCube RHI present (DX12)";
      start_present_timer();
      LOGGING(LOG_INFO, "rhi.attach role=%s mode=FlyCube/DX12 ok (retry)",
              role_name);
      sync_identity_chrome();
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
  sync_identity_chrome();
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
  // Drain Display mailbox and destroy Device before HWND teardown.
  release_rhi_device();
  stop_display_thread();
  destroy_flycube_present_hwnd();
  view_id_ = 0;
  mode_ = AttachMode::kNone;
  last_gpu_present_ok_.store(false, std::memory_order_release);
  last_content_present_ok_.store(false, std::memory_order_release);
}

void MapViewport::set_overlay_paint(OverlayPaint fn) {
  overlay_paint_ = std::move(fn);
}

void MapViewport::set_gpu_present(GpuPresentFn fn) {
  // Callback runs on the Display mailbox thread (P4), not on WM_PAINT.
  gpu_present_ = std::move(fn);
  request_frame();
}

void MapViewport::set_gpu_submit(GpuSubmitFn fn) {
  gpu_submit_ = std::move(fn);
  request_frame();
}

void MapViewport::commit_shell_overlay(const uint8_t* bgra, uint32_t width_px,
                                       uint32_t height_px,
                                       uint32_t stride_bytes,
                                       uint64_t generation,
                                       uint32_t hole_clear_argb,
                                       uint32_t hole_clear_argb_alt) {
  // Copy immediately; ShellRaster pointers must not outlive the caller.
  // FlyCube present folds this into DrawRequest.shell via snapshot + overlay.
  if (!bgra || width_px == 0 || height_px == 0) {
    return;
  }
  const uint32_t stride =
      stride_bytes != 0 ? stride_bytes : width_px * 4u;
  if (stride < width_px * 4u) {
    return;
  }
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(shell_mu_);
    if (generation != 0 &&
        generation == shell_generation_.load(std::memory_order_relaxed) &&
        shell_width_px_ == width_px && shell_height_px_ == height_px &&
        !shell_bgra_.empty()) {
      return;
    }
    shell_bgra_.resize(static_cast<size_t>(width_px) * height_px * 4u);
    for (uint32_t y = 0; y < height_px; ++y) {
      std::memcpy(shell_bgra_.data() + static_cast<size_t>(y) * width_px * 4u,
                  bgra + static_cast<size_t>(y) * stride,
                  static_cast<size_t>(width_px) * 4u);
    }
    // Native map HWNDs are skipped in shell paint; parents still bleed opaque
    // panel/shell fills into the HWND rect. Src-over of those fills on FlyCube
    // briefly shows a correct GPU map then covers it. Zero alpha for every
    // Theme chrome fill that can land in the map crop; keep real HUD pixels.
    auto punch = [](uint8_t* px, uint32_t argb) {
      if (argb == 0) {
        return;
      }
      const uint8_t r = static_cast<uint8_t>((argb >> 16) & 0xff);
      const uint8_t g = static_cast<uint8_t>((argb >> 8) & 0xff);
      const uint8_t b = static_cast<uint8_t>(argb & 0xff);
      // DIB is BGRA.
      if (px[2] == r && px[1] == g && px[0] == b) {
        px[3] = 0;
      }
    };
    const ui::views::Theme& theme = ui::views::Theme::current();
    const uint32_t hole_colors[] = {
        hole_clear_argb,
        hole_clear_argb_alt,
        theme.shell_bg,
        theme.panel_bg,
        theme.panel_header,
        theme.control_bg,
        theme.caption_bg,
        theme.map_placeholder,
    };
    for (uint32_t i = 0; i < width_px * height_px; ++i) {
      uint8_t* px = shell_bgra_.data() + static_cast<size_t>(i) * 4u;
      for (uint32_t argb : hole_colors) {
        punch(px, argb);
      }
    }
    shell_width_px_ = width_px;
    shell_height_px_ = height_px;
    shell_stride_bytes_ = width_px * 4u;
    if (generation == 0) {
      shell_generation_.fetch_add(1, std::memory_order_acq_rel);
    } else {
      shell_generation_.store(generation, std::memory_order_release);
    }
    changed = true;
  }
  // Map-region shell changed: wake BeginFrame so HUD lands in the next GPU
  // present. Chrome-only dirty must be filtered by the caller (BrowserView).
  if (changed) {
    request_frame();
  }
}

void MapViewport::note_hud_frame() {
  std::lock_guard<std::mutex> lock(hud_fps_mu_);
  hud_fps_timer_.update();
  hud_fps_.store(hud_fps_timer_.get_fps(), std::memory_order_relaxed);
}

void MapViewport::sync_identity_chrome() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  const wchar_t* role_name = L"MapEdit";
  if (role_ == Role::kMapData) {
    role_name = L"MapData";
  } else if (role_ == Role::kScene3d) {
    role_name = L"Scene3d";
  }

  // Human-readable engine id (same wording as testing/tools engine shots and
  // leftover SmartGis.exe top bar).
  wchar_t engine_id[48] = {};
  wchar_t engine[96] = {};
  if (role_ == Role::kScene3d) {
#if defined(SMT_HAS_SCENE3D_ENGINE)
    if (content::prefer_scene3d_flycube() && mode_ == AttachMode::kFlyCube) {
      wcscpy_s(engine_id, L"views-scene3d-dx12");
      wcscpy_s(engine, L"Views Scene3D (FlyCube/DX12)");
    } else if (content::prefer_scene3d_stereo_gl()) {
      // Default leftover stereo is D3D11; OpenGL is opt-in.
      bool d3d = true;
      if (const char* api = std::getenv("SMT_STEREO_API")) {
        if (_stricmp(api, "OpenGL") == 0) {
          d3d = false;
        } else if (_stricmp(api, "Direct3D") == 0) {
          d3d = true;
        }
      } else if (const char* flag = std::getenv("SMT_SCENE3D_SHOWCASE_D3D")) {
        if (flag[0] == '0' || flag[0] == 'n' || flag[0] == 'N') {
          d3d = false;
        } else if (flag[0] == '1' || flag[0] == 'y' || flag[0] == 'Y') {
          d3d = true;
        }
      }
      if (d3d) {
        wcscpy_s(engine_id, L"legacy-scene3d-d3d");
        wcscpy_s(engine, L"Legacy Scene3D (D3D11)");
      } else {
        wcscpy_s(engine_id, L"legacy-scene3d-gl");
        wcscpy_s(engine, L"Legacy Scene3D (OpenGL)");
      }
    } else if (content::prefer_scene3d_gdi()) {
      wcscpy_s(engine_id, L"views-scene3d-gdi");
      wcscpy_s(engine, L"Views Scene3D (GDI)");
    }
#endif
    if (engine[0] == L'\0') {
      if (mode_ == AttachMode::kFlyCube) {
        wcscpy_s(engine_id, L"views-scene3d-dx12");
        wcscpy_s(engine, L"Views Scene3D (FlyCube/DX12)");
      } else if (mode_ == AttachMode::kContentMapView) {
        wcscpy_s(engine_id, L"views-scene3d-content");
        wcscpy_s(engine, L"Views Scene3D (Content)");
      } else {
        wcscpy_s(engine_id, L"views-scene3d");
        wcscpy_s(engine, L"Scene3D");
      }
    }
  } else if (mode_ == AttachMode::kFlyCube) {
    wcscpy_s(engine_id, L"views-map2d-skia");
    wcscpy_s(engine, L"Views Map2D (Skia/RHI)");
  } else if (mode_ == AttachMode::kLocalDevice) {
    wcscpy_s(engine_id, L"legacy-map2d-gdi");
    wcscpy_s(engine, L"Legacy Map2D (GDI+)");
  } else if (mode_ == AttachMode::kContentMapView) {
    wcscpy_s(engine_id, L"views-map2d-content");
    wcscpy_s(engine, L"Views Map2D (Content)");
  } else if (mode_ == AttachMode::kOopRender) {
    wcscpy_s(engine_id, L"views-map2d-oop");
    wcscpy_s(engine, L"Views Map2D (OOP)");
  } else {
    wcscpy_s(engine_id, L"views-map2d");
    wcscpy_s(engine, L"Map2D");
  }

  const float fps = hud_fps_.load(std::memory_order_relaxed);
  wchar_t title[192] = {};
  _snwprintf_s(title, _TRUNCATE, L"%s · %s  Fps%.3f", role_name, engine, fps);
  SetWindowTextW(hwnd, title);

  // Match leftover SmartGis.exe: black top bar + yellow "id | Engine  Fps".
  wchar_t hud[220] = {};
  _snwprintf_s(hud, _TRUNCATE, L"%s | %s  Fps%.3f", engine_id, engine, fps);

  HWND parent = hwnd;
  if (flycube_present_hwnd_ && IsWindow(flycube_present_hwnd_) &&
      IsWindowVisible(flycube_present_hwnd_)) {
    parent = flycube_present_hwnd_;
  }
  RECT parent_rc = {};
  GetClientRect(parent, &parent_rc);
  const int bar_w = parent_rc.right > 0 ? parent_rc.right : 420;

  register_identity_hud_class();
  if (identity_badge_ &&
      (!IsWindow(identity_badge_) || identity_badge_parent_ != parent)) {
    if (IsWindow(identity_badge_)) {
      DestroyWindow(identity_badge_);
    }
    identity_badge_ = nullptr;
    identity_badge_parent_ = nullptr;
  }
  if (!identity_badge_) {
    identity_badge_ = CreateWindowExW(
        0, kIdentityHudClass, hud, WS_CHILD | WS_VISIBLE, 0, 0, bar_w,
        kIdentityHudHeight, parent, nullptr, GetModuleHandleW(nullptr),
        nullptr);
    identity_badge_parent_ = parent;
  }
  if (identity_badge_) {
    wcscpy_s(identity_hud_text_, hud);
    SetWindowLongPtrW(identity_badge_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(identity_hud_text_));
    SetWindowPos(identity_badge_, HWND_TOP, 0, 0, bar_w, kIdentityHudHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(identity_badge_, nullptr, FALSE);
  }
}

bool MapViewport::snapshot_shell_overlay(std::vector<uint8_t>* out_bgra,
                                         ui::gfx::ShellRaster* out_shell,
                                         uint64_t* out_generation) const {
  if (!out_bgra || !out_shell) {
    return false;
  }
  std::lock_guard<std::mutex> lock(shell_mu_);
  if (shell_bgra_.empty() || shell_width_px_ == 0 || shell_height_px_ == 0) {
    *out_shell = {};
    if (out_generation) {
      *out_generation = 0;
    }
    return false;
  }
  *out_bgra = shell_bgra_;
  out_shell->bgra = out_bgra->data();
  out_shell->width_px = shell_width_px_;
  out_shell->height_px = shell_height_px_;
  out_shell->stride_bytes = shell_stride_bytes_ != 0
                                ? shell_stride_bytes_
                                : shell_width_px_ * 4u;
  if (out_generation) {
    *out_generation = shell_generation_.load(std::memory_order_acquire);
  }
  return true;
}

void MapViewport::request_frame() {
  frame_request_.fetch_add(1, std::memory_order_acq_rel);
  signal_display();
  if (HWND hwnd = native_view()) {
    // Timer rule: InvalidateRect only while generation advances. BeginFrame
    // owns GPU present; this wakes ContentMapView / overlay paths.
    InvalidateRect(hwnd, nullptr, FALSE);
  }
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
    if (!session_->StartRenderProcess()) {
      session_->Shutdown();
      delete session_;
      session_ = nullptr;
      owns_session_ = false;
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

void MapViewport::ensure_display_thread() {
  std::lock_guard<std::mutex> lock(display_mu_);
  if (display_started_) {
    return;
  }
  display_stop_ = false;
  display_started_ = true;
  display_thread_ = std::thread([this]() { display_thread_main(); });
}

void MapViewport::stop_display_thread() {
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    if (!display_started_) {
      return;
    }
    display_stop_ = true;
  }
  display_cv_.notify_all();
  if (display_thread_.joinable()) {
    display_thread_.join();
  }
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    display_started_ = false;
    display_queue_.clear();
  }
}

void MapViewport::signal_display() {
  display_cv_.notify_all();
}

void MapViewport::enqueue_display_task(DisplayTask task) {
  ensure_display_thread();
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    // Coalesce resize ops; keep ordering for init/destroy.
    if (task.op == DisplayOp::kResize) {
      for (auto it = display_queue_.begin(); it != display_queue_.end();) {
        if (it->op == DisplayOp::kResize) {
          it = display_queue_.erase(it);
        } else {
          ++it;
        }
      }
    }
    display_queue_.push_back(task);
  }
  display_cv_.notify_one();
}

void MapViewport::display_run_present(uint32_t width_px, uint32_t height_px,
                                     uint32_t frame_token) {
  if (display_stop_) {
    return;
  }
  if (!rhi_device_) {
    LOGGING(LOG_WARNING, "rhi.present skip: no device token=%u", frame_token);
    return;
  }
  bool ok = false;
  if (gpu_submit_) {
    ok = gpu_submit_(rhi_device_, width_px, height_px, frame_token);
  } else if (gpu_present_) {
    ok = gpu_present_(rhi_device_, width_px, height_px);
  } else {
    LOGGING(LOG_WARNING,
            "rhi.present skip: no gpu_present/submit callback size=%ux%u",
            width_px, height_px);
    return;
  }
  const bool was_ok = last_gpu_present_ok_.load(std::memory_order_acquire);
  last_gpu_present_ok_.store(ok, std::memory_order_release);
  if (ok) {
    frame_presented_.store(frame_token, std::memory_order_release);
    note_hud_frame();
    if (!was_ok) {
      LOGGING(LOG_INFO, "rhi.present recovered size=%ux%u token=%u role=%d",
              width_px, height_px, frame_token, static_cast<int>(role_));
    }
  } else if (was_ok || frame_token <= 2) {
    // Log first failures and transitions; avoid flooding Output every frame.
    LOGGING(LOG_ERROR,
            "rhi.present FAIL size=%ux%u token=%u role=%d (DXGI SoT; no GDI "
            "BitBlt overlay)",
            width_px, height_px, frame_token, static_cast<int>(role_));
  }
  if (last_begin_frame_qpc_ != 0) {
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    const std::uint64_t qpc = static_cast<std::uint64_t>(now.QuadPart);
    if (qpc >= last_begin_frame_qpc_) {
      ui::gfx::note_begin_frame_to_present_qpc(qpc - last_begin_frame_qpc_);
    }
  }
}

void MapViewport::display_run_begin_frame() {
  // Scheduler: produce only when a frame was requested and not yet presented.
  // Steady-state orbit must not force shell full repaints (no InvalidateRect).
  LARGE_INTEGER begin_qpc = {};
  QueryPerformanceCounter(&begin_qpc);
  last_begin_frame_qpc_ = static_cast<std::uint64_t>(begin_qpc.QuadPart);
  ui::gfx::note_begin_frame_qpc(last_begin_frame_qpc_);

  if (role_ != Role::kScene3d) {
    if (const char* env = std::getenv("SMT_FORCE_GDI_MAP_OVERLAY")) {
      if (env[0] == '1' && env[1] == '\0') {
        return;
      }
    }
  }
  const uint32_t requested =
      frame_request_.load(std::memory_order_acquire);
  const uint32_t presented =
      frame_presented_.load(std::memory_order_acquire);
  if (requested == presented) {
    return;
  }
  if (!rhi_device_ || (!gpu_present_ && !gpu_submit_)) {
    return;
  }
  uint32_t w = 0;
  uint32_t h = 0;
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    w = display_client_w_ != 0 ? display_client_w_ : 1;
    h = display_client_h_ != 0 ? display_client_h_ : 1;
  }
  display_run_present(w, h, requested);
}

void MapViewport::shutdown_rhi_on_display_thread() {
#ifdef SMT_HAS_FLYCUBE
  render::rhi::Device* device = nullptr;
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    device = static_cast<render::rhi::Device*>(rhi_device_);
    rhi_device_ = nullptr;
    last_gpu_present_ok_.store(false, std::memory_order_release);
  }
  if (!device) {
    return;
  }
  device->shutdown();
#else
  rhi_device_ = nullptr;
#endif
}

void MapViewport::display_thread_main() {
  // P5: interruptible idle wait (enqueue / signal_display) + DXGI vblank
  // phase-align before BeginFrame. WaitForVBlank alone cannot be woken by
  // display_cv_, which left init/destroy and frame requests delayed or stuck
  // while 2D WM_PAINT trusted a stale last_gpu_present_ok_ clear.
  for (;;) {
    DisplayTask task;
    bool have_task = false;
    {
      std::unique_lock<std::mutex> lock(display_mu_);
      if (display_stop_ && display_queue_.empty()) {
        break;
      }
      if (display_queue_.empty() && !display_stop_) {
        display_cv_.wait_for(lock, std::chrono::milliseconds(16), [this]() {
          return display_stop_ || !display_queue_.empty();
        });
      }
      if (display_stop_ && display_queue_.empty()) {
        break;
      }
      if (!display_queue_.empty()) {
        task = display_queue_.front();
        display_queue_.pop_front();
        have_task = true;
      }
    }
    if (!have_task) {
      if (display_stop_) {
        break;
      }
      HWND hwnd = nullptr;
      {
        std::lock_guard<std::mutex> lock(display_mu_);
        if (!display_queue_.empty()) {
          continue;
        }
        hwnd = display_hwnd_;
      }
      display_vblank_.set_hwnd(hwnd);
      // Already paced ~16ms by wait_for; phase to scanout. On DXGI failure
      // Sleep(1) — do not add another full refresh of Sleep.
      display_vblank_.wait_next(1);
      {
        std::lock_guard<std::mutex> lock(display_mu_);
        if (display_stop_) {
          break;
        }
        if (!display_queue_.empty()) {
          continue;
        }
      }
      display_run_begin_frame();
      continue;
    }
#ifdef SMT_HAS_FLYCUBE
      if (task.op == DisplayOp::kInit) {
        LOGGING(LOG_INFO, "rhi.display Init hwnd=%p size=%ux%u", task.hwnd,
                task.width_px, task.height_px);
        if (rhi_device_) {
          auto* old = static_cast<render::rhi::Device*>(rhi_device_);
          old->shutdown();
          rhi_device_ = nullptr;
        }
        const render::rhi::Backend backend = render::rhi::preferred_gpu_backend();
        render::rhi::Device* device = render::rhi::create_device(backend);
        if (!device) {
          LOGGING(LOG_ERROR,
                  "rhi.display Init create_device(%s) returned null",
                  render::rhi::backend_display_name(backend));
          {
            std::lock_guard<std::mutex> lock(display_mu_);
            display_init_ = DisplayInit::kFail;
          }
          display_cv_.notify_all();
          continue;
        }
        render::rhi::DeviceDesc desc;
        desc.native_window = task.hwnd;
        desc.width = task.width_px != 0 ? task.width_px : 1;
        desc.height = task.height_px != 0 ? task.height_px : 1;
        if (!device->initialize(desc)) {
          LOGGING(LOG_ERROR,
                  "rhi.display Init initialize failed backend=%s hwnd=%p "
                  "%ux%u",
                  render::rhi::backend_display_name(backend), task.hwnd,
                  desc.width, desc.height);
          device->shutdown();
          {
            std::lock_guard<std::mutex> lock(display_mu_);
            display_init_ = DisplayInit::kFail;
          }
          display_cv_.notify_all();
          continue;
        }
        LOGGING(LOG_INFO, "rhi.display Init device ok backend=%s %ux%u",
                render::rhi::backend_display_name(device->backend()), desc.width,
                desc.height);
        render::rhi::CommandList* list = device->create_command_list();
        if (list) {
          render::rhi::RenderPassDesc pass;
          pass.clear_r = 0.05f;
          pass.clear_g = 0.12f;
          pass.clear_b = 0.18f;
          pass.clear_a = 1.f;
          pass.width = desc.width;
          pass.height = desc.height;
          list->begin_render_pass(pass);
          list->set_viewport(0, 0, static_cast<float>(desc.width),
                             static_cast<float>(desc.height), 0, 1);
          list->end_render_pass();
          list->close();
          device->execute(list);
          device->destroy_command_list(list);
        }
        device->present();
        {
          std::lock_guard<std::mutex> lock(display_mu_);
          rhi_device_ = device;
          display_client_w_ = desc.width;
          display_client_h_ = desc.height;
          display_hwnd_ = task.hwnd;
          display_vblank_.set_hwnd(task.hwnd);
          display_init_ = DisplayInit::kOk;
        }
        display_cv_.notify_all();
        // Do NOT call present_gpu here. Pre-show layout often hands a stale
        // multi-k client rect; map2d present_gpu at that size blocks the
        // Display thread so the post-attach WM_SIZE Resize never runs and the
        // HWND stays on this navy clear. First real frame is Resize below
        // (and BeginFrame). Keep last_gpu_present_ok_ false so GDI overlay
        // is not skipped while waiting.
        last_gpu_present_ok_.store(false, std::memory_order_release);
        display_cv_.notify_all();
        continue;
      }
      if (task.op == DisplayOp::kResize && rhi_device_) {
        auto* device = static_cast<render::rhi::Device*>(rhi_device_);
        render::rhi::DeviceDesc desc;
        desc.native_window = task.hwnd;
        desc.width = task.width_px != 0 ? task.width_px : 1;
        desc.height = task.height_px != 0 ? task.height_px : 1;
        device->initialize(desc);
        {
          std::lock_guard<std::mutex> lock(display_mu_);
          display_client_w_ = desc.width;
          display_client_h_ = desc.height;
          display_hwnd_ = task.hwnd;
          display_vblank_.set_hwnd(task.hwnd);
        }
        // initialize() rebuilds the flip swapchain (navy clear). Redraw
        // immediately — waiting for a later BeginFrame token left Map2d on
        // the clear after the post-attach WM_SIZE shrink.
        last_gpu_present_ok_.store(false, std::memory_order_release);
        if (gpu_present_ || gpu_submit_) {
          const uint32_t token =
              frame_request_.fetch_add(1, std::memory_order_acq_rel) + 1;
          display_run_present(desc.width, desc.height, token);
        }
        continue;
      }
      if (task.op == DisplayOp::kDestroy) {
        if (rhi_device_) {
          auto* device = static_cast<render::rhi::Device*>(rhi_device_);
          device->shutdown();
          // FlyCube CRT / allocator can heap-corrupt on operator delete after
          // a live DX12 session; leak the facade (matches rhi_test).
          rhi_device_ = nullptr;
        }
        last_gpu_present_ok_.store(false, std::memory_order_release);
        {
          std::lock_guard<std::mutex> lock(display_mu_);
          display_hwnd_ = nullptr;
          display_vblank_.reset();
          display_destroy_ack_ = true;
        }
        display_cv_.notify_all();
        continue;
      }
#else
      (void)task;
#endif
  }
  // stop_display_thread joins without a kDestroy when init was abandoned.
  // Drop the swapchain here or GDI/content present cannot cover the HWND.
  shutdown_rhi_on_display_thread();
}

void MapViewport::release_rhi_device() {
  if (!rhi_device_ && !display_started_) {
    return;
  }
  if (!display_started_) {
    if (rhi_device_) {
      auto* device = static_cast<render::rhi::Device*>(rhi_device_);
      device->shutdown();
      rhi_device_ = nullptr;
    }
    return;
  }
  // Must not run on the Display thread (would deadlock).
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    display_destroy_ack_ = false;
  }
  enqueue_display_task(DisplayTask{DisplayOp::kDestroy, nullptr, 0, 0});
  std::unique_lock<std::mutex> lock(display_mu_);
  display_cv_.wait(lock, [this]() {
    return display_destroy_ack_ || display_stop_;
  });
}

void MapViewport::destroy_flycube_present_hwnd() {
  if (flycube_present_hwnd_ && IsWindow(flycube_present_hwnd_)) {
    DestroyWindow(flycube_present_hwnd_);
  }
  flycube_present_hwnd_ = nullptr;
}

void MapViewport::set_flycube_present_visible(bool show) {
  if (!flycube_present_hwnd_ || !IsWindow(flycube_present_hwnd_)) {
    return;
  }
  if (!show) {
    ShowWindow(flycube_present_hwnd_, SW_HIDE);
    return;
  }
  // Match embed client; sibling Map-Edit present must not stay above Scene3d.
  RECT rc = {};
  if (HWND embed = native_view()) {
    GetClientRect(embed, &rc);
  }
  uint32_t w = rc.right > 0 ? static_cast<uint32_t>(rc.right) : 1;
  uint32_t h = rc.bottom > 0 ? static_cast<uint32_t>(rc.bottom) : 1;
  if (role_ == Role::kScene3d) {
    clamp_scene3d_swapchain_size(&w, &h);
  }
  sync_flycube_present_hwnd(w, h);
  ShowWindow(flycube_present_hwnd_, SW_SHOWNOACTIVATE);
  SetWindowPos(flycube_present_hwnd_, HWND_TOP, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void MapViewport::sync_flycube_present_hwnd(uint32_t width_px,
                                            uint32_t height_px) {
  HWND embed = native_view();
  if (!flycube_present_hwnd_ || !IsWindow(flycube_present_hwnd_) || !embed) {
    return;
  }
  POINT tl = {0, 0};
  ClientToScreen(embed, &tl);
  const int w = width_px > 0 ? static_cast<int>(width_px) : 1;
  const int h = height_px > 0 ? static_cast<int>(height_px) : 1;
  SetWindowPos(flycube_present_hwnd_, HWND_TOP, tl.x, tl.y, w, h,
               SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

HWND MapViewport::ensure_flycube_present_hwnd(uint32_t width_px,
                                              uint32_t height_px) {
  HWND embed = native_view();
  if (!embed) {
    return nullptr;
  }
  if (flycube_present_hwnd_ && IsWindow(flycube_present_hwnd_)) {
    sync_flycube_present_hwnd(width_px, height_px);
    return flycube_present_hwnd_;
  }
  static bool registered = false;
  HINSTANCE inst = GetModuleHandleW(nullptr);
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MapViewport::present_wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kFlyCubePresentClass;
    registered = RegisterClassExW(&wc) != 0 ||
                 GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }
  // Free top-level present (showcase pattern). Owned WS_POPUP without
  // WS_EX_NOREDIRECTIONBITMAP left a transparent hole over the embed; flip
  // DXGI requires NOREDIRECTIONBITMAP. TOOLWINDOW keeps Alt-Tab clean.
  const int w = width_px > 0 ? static_cast<int>(width_px) : 1;
  const int h = height_px > 0 ? static_cast<int>(height_px) : 1;
  flycube_present_hwnd_ = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP, kFlyCubePresentClass,
      L"SmartGIS FlyCube Present",
      WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, w, h, nullptr, nullptr,
      inst, this);
  if (!flycube_present_hwnd_) {
    LOGGING(LOG_ERROR, "rhi.flycube present HWND create failed");
    return nullptr;
  }
  SetWindowLongPtrW(flycube_present_hwnd_, GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(this));
  sync_flycube_present_hwnd(width_px, height_px);
  // Follow embed visibility — Map Edit present must not cover a later Scene3d.
  const bool embed_visible = IsWindowVisible(embed);
  ShowWindow(flycube_present_hwnd_,
             embed_visible ? SW_SHOWNOACTIVATE : SW_HIDE);
  sync_identity_chrome();
  LOGGING(LOG_INFO, "rhi.flycube present HWND=%p owner=%p %ux%u visible=%d",
          flycube_present_hwnd_, nullptr, width_px, height_px,
          embed_visible ? 1 : 0);
  return flycube_present_hwnd_;
}

LRESULT CALLBACK MapViewport::present_wnd_proc(HWND hwnd, UINT msg,
                                               WPARAM wparam, LPARAM lparam) {
  // Reuse embed input / paint routing; DXGI owns the client area.
  return child_wnd_proc(hwnd, msg, wparam, lparam);
}

bool MapViewport::try_flycube_device() {
  // Opt-out: MFC / leftover GDI still required for some hosts.
  if (const char* prefer = std::getenv("SMT_PREFER_GDI_DEVICE")) {
    if (prefer[0] == '1' && prefer[1] == '\0') {
      LOGGING(LOG_INFO, "rhi.flycube skipped: SMT_PREFER_GDI_DEVICE=1");
      return false;
    }
  }
#ifndef SMT_HAS_FLYCUBE
  LOGGING(LOG_ERROR, "rhi.flycube skipped: SMT_HAS_FLYCUBE not defined");
  return false;
#else
  HWND embed = native_view();
  if (!embed) {
    LOGGING(LOG_ERROR, "rhi.flycube fail: native_view null");
    return false;
  }
  // Match View bounds before sampling client size — attach used to run while
  // the embed was still at a stale oversized rect (multi-k px), and flip DXGI
  // then cleared navy without ever showing map/DEM content.
  sync_native_bounds();
  RECT rc = {};
  GetClientRect(embed, &rc);
  uint32_t w = rc.right > 0 ? static_cast<uint32_t>(rc.right) : 1;
  uint32_t h = rc.bottom > 0 ? static_cast<uint32_t>(rc.bottom) : 1;
  const Rect& laid_out = bounds();
  if (laid_out.width > 1 && laid_out.height > 1) {
    const int dw = static_cast<int>(w) - laid_out.width;
    const int dh = static_cast<int>(h) - laid_out.height;
    if (dw > 2 || dw < -2 || dh > 2 || dh < -2) {
      LOGGING(LOG_WARNING,
              "rhi.flycube client/bounds mismatch %ux%u vs %dx%d; using bounds",
              w, h, laid_out.width, laid_out.height);
      w = static_cast<uint32_t>(laid_out.width);
      h = static_cast<uint32_t>(laid_out.height);
      SetWindowPos(embed, nullptr, laid_out.x, laid_out.y, laid_out.width,
                   laid_out.height,
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
    }
  }
  // Cap Init to the primary monitor — pre-show layout can report a child
  // client larger than the shell briefly; DXGI at that size is fine on a
  // real 4K host but must not exceed the screen.
  {
    const uint32_t screen_w =
        static_cast<uint32_t>((std::max)(64, GetSystemMetrics(SM_CXSCREEN)));
    const uint32_t screen_h =
        static_cast<uint32_t>((std::max)(64, GetSystemMetrics(SM_CYSCREEN)));
    if (w > screen_w || h > screen_h) {
      LOGGING(LOG_WARNING,
              "rhi.flycube clamp init %ux%u to screen %ux%u", w, h, screen_w,
              screen_h);
      if (w > screen_w) {
        w = screen_w;
      }
      if (h > screen_h) {
        h = screen_h;
      }
    }
  }
  if (role_ == Role::kScene3d) {
    clamp_scene3d_swapchain_size(&w, &h);
  }
  // Flip-model DXGI on WS_CHILD clears navy but never shows map/DEM. All
  // FlyCube roles present on a top-level popup over the embed (showcase
  // pattern). Tab switch hides inactive present via set_flycube_present_visible
  // so Map-Edit cannot cover Scene3d.
  HWND hwnd = embed;
  if (HWND present = ensure_flycube_present_hwnd(w, h)) {
    hwnd = present;
  }
  LOGGING(LOG_INFO, "rhi.flycube enqueue Init hwnd=%p size=%ux%u (client=%dx%d)",
          hwnd, w, h, static_cast<int>(rc.right), static_cast<int>(rc.bottom));
  release_rhi_device();
  ensure_display_thread();
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    display_init_ = DisplayInit::kPending;
  }
  enqueue_display_task(DisplayTask{DisplayOp::kInit, hwnd, w, h});
  // Wait on the Display thread cv instead of Sleep(1)×5000 — DX12 init often
  // finishes in tens of ms; the poll made attach feel multi-second.
  {
    std::unique_lock<std::mutex> lock(display_mu_);
    const bool signaled = display_cv_.wait_for(
        lock, std::chrono::seconds(5), [this]() {
          return display_init_ == DisplayInit::kOk ||
                 display_init_ == DisplayInit::kFail;
        });
    if (signaled && display_init_ == DisplayInit::kOk) {
      LOGGING(LOG_INFO, "rhi.flycube Init ok hwnd=%p %ux%u", hwnd, w, h);
      return true;
    }
    LOGGING(LOG_ERROR,
            "rhi.flycube Init FAIL hwnd=%p %ux%u signaled=%d state=%d "
            "(0=idle,1=pending,2=ok,3=fail)",
            hwnd, w, h, signaled ? 1 : 0,
            static_cast<int>(display_init_));
  }
  stop_display_thread();
  return false;
#endif
}

bool MapViewport::try_local_device() {
  // Optional leftover DLL (dll_stem = legacy_render). gdi_simple / per-engine
  // stems were retired; CreateRenderDevice is the sole GDI factory export.
  const wchar_t* names[] = {
      L"legacy_render_d.dll",
      L"legacy_render.dll",
      nullptr,
  };
  local_module_ = load_first(names);
  if (!local_module_) {
    return false;
  }
  auto create = reinterpret_cast<CreateRenderDeviceFn>(
      GetProcAddress(local_module_, "CreateRenderDevice"));
  if (!create) {
    return false;
  }
  void* device = nullptr;
  if (create(local_module_, device) != 0 || !device) {
    return false;
  }
  if (!init_device_seh(device, native_view())) {
    return false;
  }
  local_device_ = device;
  RECT rc = {};
  GetClientRect(native_view(), &rc);
  auto* obj = static_cast<DeviceObj*>(device);
  if (obj->vtbl && obj->vtbl->Resize && rc.right > 0 && rc.bottom > 0) {
    obj->vtbl->Resize(obj, 0, 0, rc.right, rc.bottom);
  }
  return true;
}

void MapViewport::paint_child_placeholder() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  // Do not erase: TRUE would flash the class brush / default clear between
  // present timer ticks and the composited BitBlt.
  InvalidateRect(hwnd, nullptr, FALSE);
}

void MapViewport::release_backbuffer() {
  if (back_dc_) {
    if (back_old_) {
      SelectObject(back_dc_, back_old_);
      back_old_ = nullptr;
    }
    DeleteDC(back_dc_);
    back_dc_ = nullptr;
  }
  if (back_dib_) {
    DeleteObject(back_dib_);
    back_dib_ = nullptr;
  }
  back_w_ = 0;
  back_h_ = 0;
}

bool MapViewport::ensure_backbuffer(int width_px, int height_px) {
  if (width_px <= 0 || height_px <= 0) {
    return false;
  }
  if (back_dc_ && back_dib_ && back_w_ == width_px && back_h_ == height_px) {
    return true;
  }
  release_backbuffer();

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width_px;
  bmi.bmiHeader.biHeight = -height_px;  // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    return false;
  }
  HDC mem = CreateCompatibleDC(nullptr);
  if (!mem) {
    DeleteObject(dib);
    return false;
  }
  HBITMAP old = static_cast<HBITMAP>(SelectObject(mem, dib));
  back_dc_ = mem;
  back_dib_ = dib;
  back_old_ = old;
  back_w_ = width_px;
  back_h_ = height_px;
  // Size change discards the last composited frame; require a fresh present
  // (or placeholder) before BitBlt so we never show an empty DIB.
  painted_generation_ = 0;
  RECT fill = {0, 0, width_px, height_px};
  const COLORREF bg = (role_ == Role::kScene3d) ? RGB(18, 32, 48)
                                                 : RGB(170, 211, 223);
  HBRUSH brush = CreateSolidBrush(bg);
  FillRect(mem, &fill, brush);
  DeleteObject(brush);
  return true;
}

bool MapViewport::export_bmp(const std::string& path) const {
  if (path.empty() || !frame_ready_ || !back_dib_ || !back_dc_ || back_w_ <= 0 ||
      back_h_ <= 0) {
    return false;
  }
  const int width = back_w_;
  const int height = back_h_;
  const int stride = width * 4;
  std::vector<std::uint8_t> pixels(static_cast<size_t>(stride) * height);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  if (!GetDIBits(back_dc_, back_dib_, 0, static_cast<UINT>(height),
                 pixels.data(), &bmi, DIB_RGB_COLORS)) {
    return false;
  }
  const std::uint32_t pixel_bytes =
      static_cast<std::uint32_t>(stride) * static_cast<std::uint32_t>(height);
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  const std::uint32_t file_size = 54u + pixel_bytes;
  const unsigned char header[54] = {
      'B', 'M',
      static_cast<unsigned char>(file_size),
      static_cast<unsigned char>(file_size >> 8),
      static_cast<unsigned char>(file_size >> 16),
      static_cast<unsigned char>(file_size >> 24),
      0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
      static_cast<unsigned char>(width),
      static_cast<unsigned char>(width >> 8),
      static_cast<unsigned char>(width >> 16),
      static_cast<unsigned char>(width >> 24),
      static_cast<unsigned char>(height),
      static_cast<unsigned char>(height >> 8),
      static_cast<unsigned char>(height >> 16),
      static_cast<unsigned char>(height >> 24),
      1, 0, 32, 0, 0, 0, 0, 0,
      static_cast<unsigned char>(pixel_bytes),
      static_cast<unsigned char>(pixel_bytes >> 8),
      static_cast<unsigned char>(pixel_bytes >> 16),
      static_cast<unsigned char>(pixel_bytes >> 24),
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  out.write(reinterpret_cast<const char*>(header), 54);
  out.write(reinterpret_cast<const char*>(pixels.data()),
            static_cast<std::streamsize>(pixels.size()));
  return static_cast<bool>(out);
}

void MapViewport::paint_map_content(HDC target, const RECT& client_rc) {
  if (!target) {
    return;
  }
  note_hud_frame();
  bool presented = false;
  if (mode_ == AttachMode::kContentMapView) {
    presented = present_latest_frame(target, client_rc);
    last_content_present_ok_.store(presented, std::memory_order_release);
  } else {
    last_content_present_ok_.store(false, std::memory_order_release);
  }
  if (!presented && painted_generation_ > 0) {
    // Keep the last composited backbuffer; re-running overlay on top of a
    // frame that already includes vectors would stack strokes.
    return;
  }
  // Keep the previous backbuffer pixels when present briefly fails so the
  // viewport does not flash the teal placeholder between GPU generations.
  if (!presented) {
    // Pure GDI placeholder — avoid Skia Canvas on the retained mem DC (its
    // per-call BitBlt + DIB teardown has corrupted the process heap before).
    RECT fill = {0, 0, client_rc.right, client_rc.bottom};
    const bool scene3d = role_ == Role::kScene3d;
    HBRUSH brush =
        CreateSolidBrush(scene3d ? RGB(18, 32, 48) : RGB(170, 211, 223));
    FillRect(target, &fill, brush);
    DeleteObject(brush);
    SetBkMode(target, TRANSPARENT);
    SetTextColor(target, scene3d ? RGB(220, 230, 240) : RGB(60, 70, 80));
    const wchar_t* title = L"MapEdit";
    if (role_ == Role::kScene3d) {
      title = L"Scene3d";
    } else if (role_ == Role::kMapData) {
      title = L"MapData";
    }
    TextOutW(target, 16, 16, title, lstrlenW(title));
    SetTextColor(target, scene3d ? RGB(160, 200, 180) : RGB(90, 110, 100));
    const wchar_t* text = status_ ? status_ : L"Map viewport";
    TextOutW(target, 16, 40, text, lstrlenW(text));
  }
  if (overlay_paint_) {
    overlay_paint_(target, client_rc);
  }
}

void MapViewport::start_present_timer() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  SetTimer(hwnd, kPresentTimerId, 33, nullptr);
}

void MapViewport::stop_present_timer() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  KillTimer(hwnd, kPresentTimerId);
}

bool MapViewport::present_latest_frame(HDC hdc, const RECT& client_rc) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
  if (!hdc || !session_ || view_id_ == 0) {
    return false;
  }
  content::MapWidgetHostView* view = session_->HostView(view_id_);
  if (!view) {
    return false;
  }
  const content::SharedSurface surface = view->Latest();
  if (!surface.nt_handle || surface.generation == 0 || surface.width_px == 0 ||
      surface.height_px == 0) {
    return false;
  }
  // Cap blit size to avoid pathological maps / bad IPC metadata.
  if (surface.width_px > 8192u || surface.height_px > 8192u) {
    return false;
  }
  const SIZE_T bytes = static_cast<SIZE_T>(surface.width_px) *
                       static_cast<SIZE_T>(surface.height_px) * 4u;
  void* bits = MapViewOfFile(static_cast<HANDLE>(surface.nt_handle),
                             FILE_MAP_READ, 0, 0, bytes);
  if (!bits) {
    bits = MapViewOfFile(static_cast<HANDLE>(surface.nt_handle),
                         FILE_MAP_ALL_ACCESS, 0, 0, bytes);
  }
  if (!bits) {
    return false;
  }
  // Never read past the mapped region (bad IPC size metadata corrupts heap).
  MEMORY_BASIC_INFORMATION mbi = {};
  SIZE_T mapped = 0;
  if (VirtualQuery(bits, &mbi, sizeof(mbi)) != 0) {
    mapped = mbi.RegionSize;
  }
  if (mapped == 0 || mapped < bytes) {
    UnmapViewOfFile(bits);
    return false;
  }
  // SharedSurface exposes an NT section handle, not a stable heap pointer.
  // The view stays mapped only for this blit — no second std::vector on the
  // UI thread. A raster thread is intentionally not started here; see the P2
  // gate on the FlyCube present call.
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = static_cast<LONG>(surface.width_px);
  bi.bmiHeader.biHeight = -static_cast<LONG>(surface.height_px);
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  const int dst_w = client_rc.right > 0 ? client_rc.right : 1;
  const int dst_h = client_rc.bottom > 0 ? client_rc.bottom : 1;
  const int ok =
      StretchDIBits(hdc, 0, 0, dst_w, dst_h, 0, 0,
                    static_cast<int>(surface.width_px),
                    static_cast<int>(surface.height_px), bits, &bi,
                    DIB_RGB_COLORS, SRCCOPY);
  UnmapViewOfFile(bits);
  if (ok == 0 || ok == GDI_ERROR) {
    return false;
  }
  painted_generation_ = surface.generation;
  frame_ready_ = true;
  return true;
#else
  (void)hdc;
  (void)client_rc;
  return false;
#endif
}

LRESULT CALLBACK MapViewport::child_wnd_proc(HWND hwnd, UINT msg,
                                             WPARAM wparam, LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }
  auto* self =
      reinterpret_cast<MapViewport*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (msg == WM_TIMER && wparam == kPresentTimerId) {
    if (self) {
      // Refresh legacy-style engine + Fps HUD even when GPU paint skips GDI.
      self->sync_identity_chrome();
    }
    if (self && self->mode_ == AttachMode::kContentMapView) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
      if (self->role_ == Role::kScene3d && IsWindowVisible(hwnd)) {
        // Leftover stereo / GDI SoT need continuous refresh (orbit + HUD).
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (self->session_ && self->view_id_ != 0) {
        if (content::MapWidgetHostView* view =
                self->session_->HostView(self->view_id_)) {
          const content::SharedSurface surface = view->Latest();
          if (surface.generation != 0 &&
              surface.generation != self->painted_generation_) {
            InvalidateRect(hwnd, nullptr, FALSE);
          }
        }
      }
#endif
    } else if (self && self->mode_ == AttachMode::kFlyCube &&
               IsWindowVisible(hwnd) &&
               (self->role_ == Role::kScene3d ||
                self->role_ == Role::kMapEdit ||
                self->role_ == Role::kMapData)) {
      // Scene3d: keep BeginFrame pacing so ocean FFT / cloud time advance.
      // 2D roles still invalidate only while a frame token is pending.
      if (self->role_ == Role::kScene3d) {
        self->request_frame();
        return 0;
      }
      const uint32_t req =
          self->frame_request_.load(std::memory_order_acquire);
      const uint32_t presented =
          self->frame_presented_.load(std::memory_order_acquire);
      if (req != presented) {
        InvalidateRect(hwnd, nullptr, FALSE);
      }
    }
    return 0;
  }
  if (msg == WM_PAINT) {
    LARGE_INTEGER t0 = {};
    QueryPerformanceCounter(&t0);
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const int width_px = rc.right > 0 ? rc.right : 0;
    const int height_px = rc.bottom > 0 ? rc.bottom : 0;
    // FlyCube (2D MapScene or 3D Scene3d): GPU presents on the Display mailbox
    // thread via BeginFrame — never synchronously here (P4/P5).
    const bool flycube_gpu_role =
        self &&
        (self->role_ == Role::kScene3d || self->role_ == Role::kMapEdit ||
         self->role_ == Role::kMapData);
    if (flycube_gpu_role && self->mode_ == AttachMode::kFlyCube &&
        self->rhi_device_ && (self->gpu_present_ || self->gpu_submit_)) {
      const bool force_gdi_overlay = []() {
        if (const char* env = std::getenv("SMT_FORCE_GDI_MAP_OVERLAY")) {
          return env[0] == '1' && env[1] == '\0';
        }
        return false;
      }();
      // FlyCube owns the DXGI swapchain on this HWND. Never BitBlt a GDI
      // backbuffer over it — that flashes a correct GPU frame then replaces it
      // with a slow / wrong software paint. Force-GDI is the only escape hatch
      // for 2D hosts that need MapScene::paint as SoT.
      if (!force_gdi_overlay || self->role_ == Role::kScene3d) {
        {
          std::lock_guard<std::mutex> lock(self->display_mu_);
          uint32_t cw = width_px > 0 ? static_cast<uint32_t>(width_px) : 1;
          uint32_t ch = height_px > 0 ? static_cast<uint32_t>(height_px) : 1;
          // Prefer the FlyCube present popup client — embed GetClientRect can
          // stay at a stale multi-k size while the popup tracks the visible
          // tab body after show/resize.
          if (self->flycube_present_hwnd_ &&
              IsWindow(self->flycube_present_hwnd_)) {
            RECT pr = {};
            GetClientRect(self->flycube_present_hwnd_, &pr);
            if (pr.right > 0 && pr.bottom > 0) {
              cw = static_cast<uint32_t>(pr.right);
              ch = static_cast<uint32_t>(pr.bottom);
            }
          }
          if (self->role_ == Role::kScene3d) {
            clamp_scene3d_swapchain_size(&cw, &ch);
          }
          self->display_client_w_ = cw;
          self->display_client_h_ = ch;
        }
        self->signal_display();
        EndPaint(hwnd, &ps);
        LARGE_INTEGER t1 = {};
        QueryPerformanceCounter(&t1);
        if (t1.QuadPart > t0.QuadPart) {
          ui::gfx::note_map_paint_qpc(
              static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
        }
        return 0;
      }
      self->last_gpu_present_ok_.store(false, std::memory_order_release);
    }
    // Scene3d + ContentMapView (self-test / hang-safe attach): blit GPU DIB
    // first, then shell overlay (stereo / HUD). Overlay must not treat the
    // DIB as leftover stereo.
    if (self && self->role_ == Role::kScene3d &&
        self->mode_ == AttachMode::kContentMapView &&
        self->overlay_paint_ && width_px > 0 && height_px > 0) {
      if (!self->present_latest_frame(hdc, rc)) {
        RECT fill = {0, 0, rc.right, rc.bottom};
        HBRUSH brush = CreateSolidBrush(RGB(18, 32, 48));
        FillRect(hdc, &fill, brush);
        DeleteObject(brush);
      }
      self->overlay_paint_(hdc, rc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    // Scene3d SoT: leftover GL SwapBuffers (or GDI) on this HWND. A backbuffer
    // BitBlt does not contain the GL front buffer and would cover it.
    if (self && self->role_ == Role::kScene3d && self->overlay_paint_ &&
        width_px > 0 && height_px > 0) {
      self->overlay_paint_(hdc, rc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    // Map placeholder: composite present + vector overlay offscreen,
    // then one BitBlt so the user never sees a half-drawn frame.
    if (self && width_px > 0 && height_px > 0 &&
        self->ensure_backbuffer(width_px, height_px)) {
      self->paint_map_content(self->back_dc_, rc);
      BitBlt(hdc, 0, 0, width_px, height_px, self->back_dc_, 0, 0, SRCCOPY);
    } else if (self) {
      self->paint_map_content(hdc, rc);
    }
    EndPaint(hwnd, &ps);
    LARGE_INTEGER t1 = {};
    QueryPerformanceCounter(&t1);
    if (t1.QuadPart > t0.QuadPart) {
      ui::gfx::note_map_paint_qpc(static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
    }
    return 0;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  if (msg == WM_SIZE && self) {
    const int cx = static_cast<int>(LOWORD(lparam));
    const int cy = static_cast<int>(HIWORD(lparam));
    if (cx <= 0 || cy <= 0) {
      self->release_backbuffer();
    }
    if (self->mode_ == AttachMode::kFlyCube) {
      self->frame_request_.fetch_add(1, std::memory_order_acq_rel);
    }
    self->resize_host_surface(cx, cy);
    if (self->local_device_) {
      auto* obj = static_cast<DeviceObj*>(self->local_device_);
      if (obj->vtbl && obj->vtbl->Resize) {
        obj->vtbl->Resize(obj, 0, 0, cx, cy);
      }
    }
    // FlyCube resize/present stay on the Display mailbox thread (P4).
    if (self->mode_ == AttachMode::kFlyCube && cx > 0 && cy > 0) {
      uint32_t rw = static_cast<uint32_t>(cx);
      uint32_t rh = static_cast<uint32_t>(cy);
      if (self->role_ == Role::kScene3d) {
        clamp_scene3d_swapchain_size(&rw, &rh);
      }
      HWND present = self->flycube_present_hwnd_;
      if (present && IsWindow(present)) {
        // Embed WM_SIZE repositions the present popup; present WM_SIZE only
        // resizes the swapchain (avoid SetWindowPos → WM_SIZE recursion).
        if (hwnd != present) {
          self->sync_flycube_present_hwnd(rw, rh);
        }
      } else {
        present = hwnd;
      }
      self->enqueue_display_task(MapViewport::DisplayTask{
          MapViewport::DisplayOp::kResize, present, rw, rh});
      self->signal_display();
    }
  }
  if (self && (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN)) {
    SetFocus(hwnd);
  }
  if (self &&
      route_view_host_pointer(self->view_host_, hwnd, msg, wparam,
                              &self->touch_tracker_)) {
    return 0;
  }
  if (self &&
      route_view_host_input(self->view_host_, hwnd, msg, wparam, lparam,
                            self->touch_tracker_.suppress_mouse())) {
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace views
}  // namespace ui
