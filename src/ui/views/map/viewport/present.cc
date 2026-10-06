// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// ContentMapView SharedSurface blit, overlay compose, present timer.

#include "ui/views/map/viewport/draw_host.h"

#include "ui/views/map/frame/embed_fill.h"
#include "ui/views/map/viewport/features.h"
#include "ui/views/map/viewport/paint_policy.h"

namespace ui {
namespace views {

void DrawHost::paint_child_placeholder() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  // Do not erase: TRUE would flash the class brush / default clear between
  // present timer ticks and the composited BitBlt.
  InvalidateRect(hwnd, nullptr, FALSE);
}

void DrawHost::start_present_timer() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  // Seed a live cadence so identity HUD is not stuck at Fps0.000 until the
  // first note_hud_frame sample (ContentMapView idle gaps are common).
  if (hud_fps_.load(std::memory_order_relaxed) < 1.0e-3f) {
    hud_fps_.store(60.f, std::memory_order_relaxed);
  }
  present_paused_.store(false, std::memory_order_release);
  SetTimer(hwnd, kPresentTimerId, 16, nullptr);
}

void DrawHost::pause_present() {
  present_paused_.store(true, std::memory_order_release);
  set_gpu_present_visible(false);
  stop_present_timer();
}

void DrawHost::resume_present_timer() {
  present_paused_.store(false, std::memory_order_release);
  start_present_timer();
  request_frame();
}

void DrawHost::stop_present_timer() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  KillTimer(hwnd, kPresentTimerId);
}

bool DrawHost::present_latest_frame(HDC hdc, const RECT& client_rc) {
#ifdef HAS_CONTENT_MAP_SESSION
  // 2D leftover GPU SharedSurface is a demo tessellation (orange/cyan grid),
  // not Vista/Scenic china carto. Map2dPresenter overlay is the 2D SoT.
  if (role_ != Role::kScene3d) {
    return false;
  }
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

void DrawHost::paint_host_content(HDC target, const RECT& client_rc) {
  if (!target) {
    return;
  }
  note_hud_frame();
  maybe_sync_identity_hud();
  // Overlay paints must keep the 16ms HUD/present timer alive (harness
  // stop_map_present_timers / missed SetTimer leaves Fps0.000 on Content).
  if (mode_ == AttachMode::kContentMapView) {
    start_present_timer();
  }
  bool presented = false;
  if (mode_ == AttachMode::kContentMapView && role_ == Role::kScene3d) {
    presented = present_latest_frame(target, client_rc);
    last_content_present_ok_.store(presented, std::memory_order_release);
  } else {
    last_content_present_ok_.store(false, std::memory_order_release);
  }
  const bool force_gdi = detail::force_gdi_map_overlay();
  // Keep the last SharedSurface blit when 3D present briefly fails — but
  // never skip GDI overlay under FORCE_GDI_MAP_OVERLAY.
  if (!presented && painted_generation_ > 0 && !force_gdi &&
      role_ == Role::kScene3d) {
    return;
  }
  if (!presented && !force_gdi && role_ == Role::kScene3d) {
    detail::fill_map_embed_opaque(target, client_rc, /*scene3d=*/true);
  }
  if (auto paint =
          std::atomic_load_explicit(&overlay_paint_, std::memory_order_acquire);
      paint && *paint) {
    (*paint)(target, client_rc);
  }
  // 2D overlay had a chance (even if Map2dPresenter no-ops before bind).
  if (role_ != Role::kScene3d) {
    frame_ready_ = true;
  }
}

}  // namespace views
}  // namespace ui
