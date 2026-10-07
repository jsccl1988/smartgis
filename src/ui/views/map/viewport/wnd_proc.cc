// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Child / GPU present HWND: timer, paint, size, capture, input.

#include "ui/views/map/viewport/draw_host.h"

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/map/device/device_load.h"
#include "ui/views/map/frame/embed_fill.h"
#include "ui/views/map/input/viewport_input.h"
#include "ui/views/map/viewport/features.h"
#include "ui/views/map/viewport/paint_policy.h"

namespace ui {
namespace views {

void DrawHost::publish_display_client_size(uint32_t width_px,
                                              uint32_t height_px) {
  std::lock_guard<std::mutex> lock(display_mu_);
  uint32_t cw = width_px > 0 ? width_px : 1;
  uint32_t ch = height_px > 0 ? height_px : 1;
  if (gpu_present_hwnd_ && IsWindow(gpu_present_hwnd_)) {
    RECT pr = {};
    GetClientRect(gpu_present_hwnd_, &pr);
    if (pr.right > 0 && pr.bottom > 0) {
      cw = static_cast<uint32_t>(pr.right);
      ch = static_cast<uint32_t>(pr.bottom);
    }
  }
  if (role_ == Role::kScene3d) {
    detail::clamp_scene3d_swapchain_size(&cw, &ch);
  }
  display_client_w_ = cw;
  display_client_h_ = ch;
}

void DrawHost::handle_present_timer(HWND hwnd) {
  if (present_paused_.load(std::memory_order_acquire)) {
    return;
  }
  note_hud_frame();
  maybe_sync_identity_hud();
  if (mode_ == AttachMode::kContentMapView) {
#ifdef HAS_CONTENT_MAP_SESSION
    if (role_ == Role::kScene3d && IsWindowVisible(hwnd)) {
      InvalidateRect(hwnd, nullptr, FALSE);
      return;
    }
    // 2D: Map2dPresenter overlay is SoT. Waiting on leftover SharedSurface
    // generation left Fps0 + teal until a demo tessellation arrived.
    if (IsWindowVisible(hwnd)) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
#endif
    return;
  }
  if (mode_ == AttachMode::kGpuPresent && IsWindowVisible(hwnd) &&
      (role_ == Role::kScene3d || role_ == Role::kMapEdit ||
       role_ == Role::kMapData)) {
    request_frame();
    return;
  }
  if (mode_ == AttachMode::kPlaceholder && IsWindowVisible(hwnd)) {
    static DWORD last_ph_paint = 0;
    const DWORD now_paint = GetTickCount();
    if (last_ph_paint == 0 || now_paint - last_ph_paint >= 80u) {
      last_ph_paint = now_paint;
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    note_hud_frame();
  }
}

LRESULT DrawHost::handle_paint(HWND hwnd) {
  LARGE_INTEGER t0 = {};
  QueryPerformanceCounter(&t0);
  PAINTSTRUCT ps = {};
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT rc = {};
  GetClientRect(hwnd, &rc);
  const int width_px = rc.right > 0 ? rc.right : 0;
  const int height_px = rc.bottom > 0 ? rc.bottom : 0;
  const bool is_embed = hwnd == native_view();
  const bool scene3d = role_ == Role::kScene3d;
  const bool gpu_present_role =
      role_ == Role::kScene3d || role_ == Role::kMapEdit ||
      role_ == Role::kMapData;
  if (gpu_present_role && mode_ == AttachMode::kGpuPresent &&
      has_gpu_cb_.load(std::memory_order_acquire)) {
    if (!rhi_device_) {
      if (is_embed) {
        detail::fill_map_embed_opaque(hdc, rc, scene3d);
      }
      signal_display();
      EndPaint(hwnd, &ps);
      return 0;
    }
    const bool force_gdi_overlay = detail::force_gdi_map_overlay();
    if (!force_gdi_overlay || role_ == Role::kScene3d) {
      publish_display_client_size(
          width_px > 0 ? static_cast<uint32_t>(width_px) : 1,
          height_px > 0 ? static_cast<uint32_t>(height_px) : 1);
      if (is_embed) {
        detail::fill_map_embed_opaque(hdc, rc, scene3d);
      }
      signal_display();
      EndPaint(hwnd, &ps);
      LARGE_INTEGER t1 = {};
      QueryPerformanceCounter(&t1);
      if (t1.QuadPart > t0.QuadPart) {
        ui::gfx::note_map_paint_qpc(
            static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
      }
      return 0;
    }
    last_gpu_present_ok_.store(false, std::memory_order_release);
  }
  std::shared_ptr<OverlayPaint> scene_overlay;
  if (role_ == Role::kScene3d && width_px > 0 && height_px > 0) {
    scene_overlay = std::atomic_load_explicit(&overlay_paint_,
                                              std::memory_order_acquire);
  }
  if (role_ == Role::kScene3d && width_px > 0 && height_px > 0 &&
      mode_ != AttachMode::kGpuPresent) {
    if (mode_ == AttachMode::kContentMapView && scene_overlay &&
        *scene_overlay) {
      const bool presented = present_latest_frame(hdc, rc);
      last_content_present_ok_.store(presented, std::memory_order_release);
      bool have_shell_quad = false;
      {
        std::lock_guard<std::mutex> lock(shell_mu_);
        have_shell_quad = !shell_bgra_.empty() && shell_width_px_ > 0;
      }
      if (detail::force_gdi_shell_overlay() || !have_shell_quad) {
        (*scene_overlay)(hdc, rc);
      }
      EndPaint(hwnd, &ps);
      return 0;
    }
    HDC target = hdc;
    const bool offscreen = ensure_backbuffer(width_px, height_px);
    if (offscreen) {
      target = back_dc_;
    }
    detail::fill_map_embed_opaque(target, rc, /*scene3d=*/true);
    if (scene_overlay && *scene_overlay) {
      (*scene_overlay)(target, rc);
    }
    note_hud_frame();
    if (offscreen) {
      BitBlt(hdc, 0, 0, width_px, height_px, back_dc_, 0, 0, SRCCOPY);
    }
    EndPaint(hwnd, &ps);
    LARGE_INTEGER t1 = {};
    QueryPerformanceCounter(&t1);
    if (t1.QuadPart > t0.QuadPart) {
      ui::gfx::note_map_paint_qpc(
          static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
    }
    return 0;
  }
  if (width_px > 0 && height_px > 0 && ensure_backbuffer(width_px, height_px)) {
    paint_host_content(back_dc_, rc);
    BitBlt(hdc, 0, 0, width_px, height_px, back_dc_, 0, 0, SRCCOPY);
  } else {
    if (is_embed) {
      detail::fill_map_embed_opaque(hdc, rc, scene3d);
    }
    paint_host_content(hdc, rc);
  }
  EndPaint(hwnd, &ps);
  LARGE_INTEGER t1 = {};
  QueryPerformanceCounter(&t1);
  if (t1.QuadPart > t0.QuadPart) {
    ui::gfx::note_map_paint_qpc(
        static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
  }
  return 0;
}

void DrawHost::handle_embed_move(HWND hwnd) {
  if (hwnd != native_view()) {
    return;
  }
  if (!gpu_present_hwnd_ || !IsWindow(gpu_present_hwnd_) ||
      !gpu_present_want_visible_.load(std::memory_order_acquire)) {
    return;
  }
  RECT erc = {};
  GetClientRect(hwnd, &erc);
  uint32_t rw = erc.right > 0 ? static_cast<uint32_t>(erc.right) : 1;
  uint32_t rh = erc.bottom > 0 ? static_cast<uint32_t>(erc.bottom) : 1;
  if (role_ == Role::kScene3d) {
    detail::clamp_scene3d_swapchain_size(&rw, &rh);
  }
  sync_gpu_present_hwnd(rw, rh);
}

void DrawHost::handle_size(HWND hwnd, int cx, int cy) {
  if (cx <= 0 || cy <= 0) {
    release_backbuffer();
  }
  if (mode_ == AttachMode::kGpuPresent && cx > 0 && cy > 0) {
    uint32_t rw = static_cast<uint32_t>(cx);
    uint32_t rh = static_cast<uint32_t>(cy);
    if (role_ == Role::kScene3d) {
      detail::clamp_scene3d_swapchain_size(&rw, &rh);
    }
    {
      std::lock_guard<std::mutex> lock(display_mu_);
      // Same swapchain extent: ignore no-op WM_SIZE (sync_draw_host_after_resize
      // / SWP_FRAMECHANGED). Clearing shell + DXGI reinit punched StaticReuse
      // every time and flapped shell_generation.
      if (display_init_ == DisplayInit::kOk && rhi_device_ &&
          display_client_w_ == rw && display_client_h_ == rh) {
        // Scene3d Init presents a navy clear and deliberately skips
        // present_gpu. Lazy attach then posts WM_SIZE at the same client
        // size — a hard return left BeginFrame starved when the present
        // timer had not yet advanced frame_request_ (HUD stuck on soft
        // GDI / Fps~0). Wake the mailbox for the first real present_gpu.
        if (!last_gpu_present_ok_.load(std::memory_order_acquire)) {
          frame_request_.fetch_add(1, std::memory_order_acq_rel);
          signal_display();
        }
        return;
      }
    }
  }
  if (cx > 0 && cy > 0) {
    clear_shell_overlay();
  }
  if (mode_ == AttachMode::kGpuPresent) {
    frame_request_.fetch_add(1, std::memory_order_acq_rel);
  }
  resize_host_surface(cx, cy);
  if (local_device_) {
    auto* obj = static_cast<detail::DeviceObj*>(local_device_);
    if (obj->vtbl && obj->vtbl->Resize) {
      obj->vtbl->Resize(obj, 0, 0, cx, cy);
    }
  }
  if (mode_ == AttachMode::kGpuPresent && cx > 0 && cy > 0) {
    uint32_t rw = static_cast<uint32_t>(cx);
    uint32_t rh = static_cast<uint32_t>(cy);
    if (role_ == Role::kScene3d) {
      detail::clamp_scene3d_swapchain_size(&rw, &rh);
    }
    HWND present = gpu_present_hwnd_;
    if (present && IsWindow(present)) {
      if (hwnd != present) {
        sync_gpu_present_hwnd(rw, rh);
      }
    } else {
      present = hwnd;
    }
    enqueue_display_task(DisplayTask{DisplayOp::kResize, present, rw, rh});
    signal_display();
  } else if (cx > 0 && cy > 0) {
    InvalidateRect(hwnd, nullptr, FALSE);
  }
}

bool DrawHost::handle_mouse_capture(HWND hwnd, UINT msg, WPARAM wparam,
                                       LPARAM lparam) {
  if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN) {
    POINT screen = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    ClientToScreen(hwnd, &screen);
    RECT crc = {};
    GetClientRect(hwnd, &crc);
    POINT tl = {crc.left, crc.top};
    POINT br = {crc.right, crc.bottom};
    ClientToScreen(hwnd, &tl);
    ClientToScreen(hwnd, &br);
    const RECT screen_client = {tl.x, tl.y, br.x, br.y};
    if (!PtInRect(&screen_client, screen)) {
      if (GetCapture() == hwnd) {
        ReleaseCapture();
      }
      HWND hit = WindowFromPoint(screen);
      if (hit && hit != hwnd && IsWindow(hit)) {
        POINT client = screen;
        ScreenToClient(hit, &client);
        if (HWND root = GetAncestor(hit, GA_ROOT)) {
          SetForegroundWindow(root);
        }
        PostMessageW(hit, msg, wparam, MAKELPARAM(client.x, client.y));
        return true;
      }
    }
    SetFocus(hwnd);
    SetCapture(hwnd);
    return false;
  }
  if (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP || msg == WM_MBUTTONUP ||
      msg == WM_CAPTURECHANGED) {
    if (msg != WM_CAPTURECHANGED && GetCapture() == hwnd) {
      ReleaseCapture();
    }
  }
  return false;
}

LRESULT CALLBACK DrawHost::child_wnd_proc(HWND hwnd, UINT msg,
                                             WPARAM wparam, LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }
  auto* self =
      reinterpret_cast<DrawHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!self) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  if (msg == WM_TIMER && wparam == kPresentTimerId) {
    self->handle_present_timer(hwnd);
    return 0;
  }
  if (msg == WM_PAINT) {
    return self->handle_paint(hwnd);
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  if (msg == WM_MOVE || msg == WM_WINDOWPOSCHANGED) {
    self->handle_embed_move(hwnd);
  }
  if (msg == WM_SIZE) {
    self->handle_size(hwnd, static_cast<int>(LOWORD(lparam)),
                      static_cast<int>(HIWORD(lparam)));
  }
  if (self->handle_mouse_capture(hwnd, msg, wparam, lparam)) {
    return 0;
  }
  if (detail::route_tool_session_pointer(self->tool_session_, hwnd, msg, wparam,
                                      &self->touch_tracker_)) {
    return 0;
  }
  if (detail::route_tool_session_input(self->tool_session_, hwnd, msg, wparam, lparam,
                                    self->touch_tracker_.suppress_mouse())) {
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace views
}  // namespace ui
