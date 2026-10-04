// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Software backbuffer, ContentMapView blit, present timer, child WndProc.

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
#include "ui/views/map/frame/identity_hud.h"
#include "ui/views/map/device/device_load.h"
#include "ui/views/map/frame/embed_fill.h"
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
  const bool force_gdi = []() {
    if (const char* env = std::getenv("SMT_FORCE_GDI_MAP_OVERLAY")) {
      return env[0] == '1' && env[1] == '\0';
    }
    return false;
  }();
  // Keep the last SharedSurface blit when present briefly fails — but never
  // skip GDI overlay under SMT_FORCE_GDI_MAP_OVERLAY. Ocean-only / stale
  // SharedSurface + early return left the embed near-black so pan looked dead.
  if (!presented && painted_generation_ > 0 && !force_gdi) {
    return;
  }
  // Keep the previous backbuffer pixels when present briefly fails so the
  // viewport does not flash the teal placeholder between GPU generations.
  if (!presented && !force_gdi) {
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
  if (auto paint =
          std::atomic_load_explicit(&overlay_paint_, std::memory_order_acquire);
      paint && *paint) {
    (*paint)(target, client_rc);
  }
}

void MapViewport::start_present_timer() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  SetTimer(hwnd, kPresentTimerId, 16, nullptr);
}

void MapViewport::pause_present() {
  set_flycube_present_visible(false);
  stop_present_timer();
  HWND hwnd = native_view();
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  MSG msg;
  while (PeekMessageW(&msg, hwnd, WM_TIMER, WM_TIMER, PM_REMOVE)) {
    if (msg.wParam != kPresentTimerId) {
      PostMessageW(hwnd, msg.message, msg.wParam, msg.lParam);
    }
  }
}

void MapViewport::resume_present_timer() {
  start_present_timer();
  request_frame();
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
      // HUD text is human-facing; 4 Hz is enough and avoids SetWindowText /
      // badge invalidate on every 16 ms present tick.
      const DWORD now = GetTickCount();
      if (self->last_hud_sync_tick_ == 0 ||
          now - self->last_hud_sync_tick_ >= 250u) {
        self->last_hud_sync_tick_ = now;
        self->sync_identity_frame();
      }
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
      // All FlyCube roles must bump frame_request_. Scene3d already did;
      // MapEdit/MapData only InvalidateRect'd when req!=presented, so the
      // token never advanced after async Init → navy clear + Fps0 forever
      // (browse / interact loop captures).
      self->request_frame();
      return 0;
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
    const bool is_embed = self && hwnd == self->native_view();
    const bool scene3d = self && self->role_ == Role::kScene3d;
    // FlyCube (2D MapScene or 3D Scene3d): GPU presents on the Display mailbox
    // thread via BeginFrame — never synchronously here (P4/P5).
    const bool flycube_gpu_role =
        self &&
        (self->role_ == Role::kScene3d || self->role_ == Role::kMapEdit ||
         self->role_ == Role::kMapData);
    if (flycube_gpu_role && self->mode_ == AttachMode::kFlyCube &&
        self->has_gpu_cb_.load(std::memory_order_acquire)) {
      // Async attach returns FlyCube before Display Init sets rhi_device_.
      // Falling through to paint_map_content GDI-rasterizes china_city under
      // Widget::show → UpdateWindow and can stall Browser::show for seconds.
      if (!self->rhi_device_) {
        if (is_embed) {
          fill_map_embed_opaque(hdc, rc, scene3d);
        }
        self->signal_display();
        EndPaint(hwnd, &ps);
        return 0;
      }
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
        // Always opaque-fill the embed before EndPaint. Hidden present, or a
        // visible NOREDIRECTIONBITMAP popup that has not Present'd yet, must
        // not leave a WS_CLIPCHILDREN desktop hole under the map client.
        if (is_embed) {
          fill_map_embed_opaque(hdc, rc, scene3d);
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
    std::shared_ptr<OverlayPaint> scene_overlay;
    if (self && self->role_ == Role::kScene3d && width_px > 0 && height_px > 0) {
      scene_overlay = std::atomic_load_explicit(&self->overlay_paint_,
                                                std::memory_order_acquire);
    }
    // Scene3d + ContentMapView (self-test / hang-safe attach): blit GPU DIB
    // first, then shell overlay (stereo / HUD). Overlay must not treat the
    // DIB as leftover stereo. Skip GDI overlay_paint when a shell BGRA overlay
    // is already staged for DrawRequest.shell (U3 HUD-as-quad), unless
    // SMT_FORCE_GDI_SHELL_OVERLAY=1.
    if (self && self->role_ == Role::kScene3d &&
        self->mode_ == AttachMode::kContentMapView && scene_overlay &&
        *scene_overlay) {
      // Opaque underlay first — present_latest_frame failure must not leave a
      // hole (prefer_flycube_2d=0 ContentMapView fallback).
      fill_map_embed_opaque(hdc, rc, /*scene3d=*/true);
      self->present_latest_frame(hdc, rc);
      const bool force_gdi_shell = []() {
        if (const char* env = std::getenv("SMT_FORCE_GDI_SHELL_OVERLAY")) {
          return env[0] == '1' && env[1] == '\0';
        }
        return false;
      }();
      bool have_shell_quad = false;
      {
        std::lock_guard<std::mutex> lock(self->shell_mu_);
        have_shell_quad = !self->shell_bgra_.empty() && self->shell_width_px_ > 0;
      }
      if (force_gdi_shell || !have_shell_quad) {
        (*scene_overlay)(hdc, rc);
      }
      EndPaint(hwnd, &ps);
      return 0;
    }
    // Scene3d SoT: leftover GL SwapBuffers (or GDI) on this HWND. A backbuffer
    // BitBlt does not contain the GL front buffer and would cover it.
    if (self && self->role_ == Role::kScene3d && scene_overlay &&
        *scene_overlay) {
      (*scene_overlay)(hdc, rc);
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
      // Direct-to-screen path: paint_map_content may early-return when a prior
      // generation exists — fill first so ContentMapView never ends with a hole.
      if (is_embed) {
        fill_map_embed_opaque(hdc, rc, scene3d);
      }
      self->paint_map_content(hdc, rc);
    } else if (is_embed) {
      fill_map_embed_opaque(hdc, rc, /*scene3d=*/false);
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
  if (self && (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
               msg == WM_MBUTTONDOWN)) {
    // Stuck capture (lost mouse-up under DXGI focus races) routes later
    // button-downs outside the present rect back to this HWND — including
    // Map/Data/3D TabStrip clicks on the shell. Retarget those to the real
    // hit window; in-rect downs keep SetCapture so map pan still works.
    POINT screen = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    ClientToScreen(hwnd, &screen);
    RECT wr = {};
    GetWindowRect(hwnd, &wr);
    if (!PtInRect(&wr, screen)) {
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
        return 0;
      }
    }
    SetFocus(hwnd);
    // Capture so pan/drag keeps receiving WM_MOUSEMOVE after leaving the
    // client (FlyCube present is a top-level popup; without capture, moves
    // go to the shell chrome and view.pan stalls).
    SetCapture(hwnd);
  }
  if (self && (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP ||
               msg == WM_MBUTTONUP || msg == WM_CAPTURECHANGED)) {
    if (msg != WM_CAPTURECHANGED && GetCapture() == hwnd) {
      ReleaseCapture();
    }
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
