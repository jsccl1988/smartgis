// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// FlyCube present HWND ownership + try_flycube / try_local attach backends.

#include "ui/views/map/viewport/map_viewport.h"

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
#include "ui/views/map/chrome/identity_hud.h"
#include "ui/views/map/device/device_load.h"
#include "ui/views/map/chrome/embed_fill.h"
#include "ui/views/map/viewport/features.h"
#include "ui/views/map/input/viewport_input.h"

namespace ui {
namespace views {

using detail::CreateRenderDeviceFn;
using detail::DeviceObj;
using detail::clamp_scene3d_swapchain_size;
using detail::exe_dir;
using detail::file_exists;
using detail::fill_map_embed_opaque;
using detail::init_device_seh;
using detail::load_first;
using detail::register_identity_hud_class;
using detail::route_view_host_input;
using detail::route_view_host_pointer;

namespace {

// Top-level FlyCube DXGI target (flip-model fails on WS_CHILD embed panes).
const wchar_t kFlyCubePresentClass[] = L"SmartGisFlyCubePresent";

// Present is a free WS_POPUP. HWND_TOP loses to a TOPMOST shell (agent harness
// / review capture), burying the DXGI map under the ocean-colored embed fill.
HWND present_z_insert_after(HWND embed) {
  HWND root = embed ? ::GetAncestor(embed, GA_ROOT) : nullptr;
  if (root &&
      (::GetWindowLongPtrW(root, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0) {
    return HWND_TOPMOST;
  }
  return HWND_TOP;
}

}  // namespace

HWND MapViewport::input_hwnd() const {
  if (flycube_present_hwnd_ && IsWindow(flycube_present_hwnd_) &&
      IsWindowVisible(flycube_present_hwnd_)) {
    return flycube_present_hwnd_;
  }
  return native_view();
}

void MapViewport::destroy_flycube_present_hwnd() {
  flycube_present_want_visible_.store(false, std::memory_order_release);
  if (flycube_present_hwnd_ && IsWindow(flycube_present_hwnd_)) {
    DestroyWindow(flycube_present_hwnd_);
  }
  flycube_present_hwnd_ = nullptr;
}

void MapViewport::reveal_flycube_present_if_ready() {
  if (!flycube_present_want_visible_.load(std::memory_order_acquire)) {
    return;
  }
  if (!flycube_present_hwnd_ || !IsWindow(flycube_present_hwnd_)) {
    return;
  }
  DisplayInit init = DisplayInit::kIdle;
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    init = display_init_;
  }
  // Init already device->present()'d a navy clear. Showing earlier leaves a
  // WS_EX_NOREDIRECTIONBITMAP desktop hole (startup / tab-switch hollow).
  if (init != DisplayInit::kOk) {
    ShowWindow(flycube_present_hwnd_, SW_HIDE);
    return;
  }
  // Init may have used a pre-layout client (multi-k px). Re-sync to the embed
  // before ShowWindow so the DXGI popup cannot cover Catalog / chrome.
  RECT rc = {};
  if (HWND embed = native_view()) {
    GetClientRect(embed, &rc);
  }
  uint32_t w = rc.right > 0 ? static_cast<uint32_t>(rc.right) : 1;
  uint32_t h = rc.bottom > 0 ? static_cast<uint32_t>(rc.bottom) : 1;
  if (role_ == Role::kScene3d) {
    clamp_scene3d_swapchain_size(&w, &h);
  }
  const Rect& laid_out = bounds();
  if (laid_out.width > 1 && laid_out.height > 1) {
    w = static_cast<uint32_t>(laid_out.width);
    h = static_cast<uint32_t>(laid_out.height);
  }
  sync_flycube_present_hwnd(w, h);
  ShowWindow(flycube_present_hwnd_, SW_SHOWNOACTIVATE);
  SetWindowPos(flycube_present_hwnd_, present_z_insert_after(native_view()), 0,
               0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void MapViewport::set_flycube_present_visible(bool show) {
  flycube_present_want_visible_.store(show, std::memory_order_release);
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
  reveal_flycube_present_if_ready();
  // Tab show alone does not advance frame_request_; without this the DXGI
  // popup can sit on the last clear while the embed looks hollow.
  request_frame();
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
  // Never SWP_SHOWWINDOW here — that reopened the NOREDIRECTION hole before
  // Init Present. Visibility is owned by reveal_flycube_present_if_ready.
  UINT flags = SWP_NOACTIVATE;
  if (!flycube_present_want_visible_.load(std::memory_order_acquire) ||
      !IsWindowVisible(flycube_present_hwnd_)) {
    flags |= SWP_NOZORDER;
  }
  SetWindowPos(flycube_present_hwnd_, present_z_insert_after(embed), tl.x, tl.y,
               w, h, flags);
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
  // Owned top-level present (showcase pattern). Owner = shell root so the
  // popup stays above the embed, follows minimize/close, and keeps mouse
  // activation with the product frame. Unowned popups lost z-order / focus
  // under plain (no-arg) launch so pan/wheel/click never reached ViewHost.
  // WS_EX_NOREDIRECTIONBITMAP is required for flip DXGI; TOOLWINDOW keeps
  // Alt-Tab clean.
  const int w = width_px > 0 ? static_cast<int>(width_px) : 1;
  const int h = height_px > 0 ? static_cast<int>(height_px) : 1;
  HWND owner = GetAncestor(embed, GA_ROOT);
  if (!owner || !IsWindow(owner)) {
    owner = embed;
  }
  flycube_present_hwnd_ = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP, kFlyCubePresentClass,
      L"SmartGIS FlyCube Present",
      WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, w, h, owner, nullptr,
      inst, this);
  if (!flycube_present_hwnd_) {
    LOGGING(LOG_ERROR, "rhi.flycube present HWND create failed");
    return nullptr;
  }
  SetWindowLongPtrW(flycube_present_hwnd_, GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(this));
  sync_flycube_present_hwnd(width_px, height_px);
  // Prefer reveal-after-Init. Embed IsWindowVisible is often false during
  // Browser::init (pre-show); inactive tabs call set_flycube_present_visible(false).
  flycube_present_want_visible_.store(true, std::memory_order_release);
  ShowWindow(flycube_present_hwnd_, SW_HIDE);
  sync_identity_chrome();
  LOGGING(LOG_INFO, "rhi.flycube present HWND=%p owner=%p %ux%u want_visible=1",
          flycube_present_hwnd_, nullptr, width_px, height_px);
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
  // Wait for DX12 Init. Debug D3D12 layers + first adapter enumeration often
  // exceed a short UI budget; falling through to ContentMapView/GDI leaves
  // ui.scene as views-scene3d.gdi Fps0 and browse.3d without a real present.
  {
    std::unique_lock<std::mutex> lock(display_mu_);
    const int wait_ms = (role_ == Role::kScene3d) ? 2500 : 800;
    const bool signaled = display_cv_.wait_for(
        lock, std::chrono::milliseconds(wait_ms), [this]() {
          return display_init_ == DisplayInit::kOk ||
                 display_init_ == DisplayInit::kFail;
        });
    if (signaled && display_init_ == DisplayInit::kOk) {
      LOGGING(LOG_INFO, "rhi.flycube Init ok hwnd=%p %ux%u", hwnd, w, h);
      // reveal_flycube_present_if_ready lock_guards display_mu_ — must not
      // call while this unique_lock is held (MSVC throws resource_deadlock).
      lock.unlock();
      reveal_flycube_present_if_ready();
      return true;
    }
    // Still pending after the UI budget: keep FlyCube attach and finish
    // Init on the Display thread. Falling through to ContentMapView would
    // dual-SoT flash; Init Fail still tears down below.
    if (!signaled && display_init_ == DisplayInit::kPending) {
      LOGGING(LOG_INFO,
              "rhi.flycube Init still pending hwnd=%p %ux%u — async attach",
              hwnd, w, h);
      lock.unlock();
      reveal_flycube_present_if_ready();
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
  // Leftover 2D device DLL (traits paint lane). CreateRenderDevice export.
  // SMT_RHI2D_PORT=gdi|gdiplus|skia selects the LoadLibrary stem (default gdi).
  const wchar_t* names_gdi[] = {
      L"legacy_rhi2d_gdi_d.dll",
      L"legacy_rhi2d_gdi.dll",
      nullptr,
  };
  const wchar_t* names_gdiplus[] = {
      L"legacy_rhi2d_gdiplus_d.dll",
      L"legacy_rhi2d_gdiplus.dll",
      nullptr,
  };
  const wchar_t* names_skia[] = {
      L"legacy_rhi2d_skia_d.dll",
      L"legacy_rhi2d_skia.dll",
      nullptr,
  };
  const wchar_t* const* names = names_gdi;
  if (const char* port = std::getenv("SMT_RHI2D_PORT")) {
    if (_stricmp(port, "gdiplus") == 0 || _stricmp(port, "gdi+") == 0) {
      names = names_gdiplus;
    } else if (_stricmp(port, "skia") == 0) {
      names = names_skia;
    }
  }
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

}  // namespace views
}  // namespace ui
