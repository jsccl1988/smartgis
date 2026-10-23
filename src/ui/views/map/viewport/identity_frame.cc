// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// On-client identity HUD + FPS sampling for DrawHost.

#include "ui/views/map/viewport/draw_host.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/process/switches.h"
#include "ui/views/map/frame/identity_hud.h"
#include "ui/views/map/viewport/detail/engine_identity.h"
#include "ui/views/map/viewport/paint_policy.h"

namespace ui {
namespace views {

using detail::kIdentityHudClass;
using detail::kIdentityHudHeight;
using detail::register_identity_hud_class;
using detail::resolve_engine_identity;
using detail::show_map_identity_hud;

void DrawHost::publish_present_fps(float fps) {
  if (!(fps > 0.f) || !std::isfinite(fps)) {
    return;
  }
  hud_fps_.store(fps, std::memory_order_relaxed);
}

void DrawHost::note_hud_frame() {
  float fps = 0.f;
  {
    std::lock_guard<std::mutex> lock(hud_fps_mu_);
    hud_fps_timer_.update();
    // Instantaneous 1/dt collapses after idle gaps (Content Map2D only paints
    // on generation change + sporadic capture WM_PAINT). Cap dt so the HUD
    // reports a live cadence instead of Fps0.000 / 1/wall.
    const float dt = hud_fps_timer_.get_elapsed();
    constexpr float kMaxSampleDt = 0.25f;
    constexpr float kAlpha = 0.25f;
    const float prev = hud_fps_.load(std::memory_order_relaxed);
    if (dt > 1.0e-4f) {
      // Cap the sample so a first present after attach/idle (dt often seconds)
      // does not lock the HUD at Fps0.000 / 1/dt. Timer and overlay paints are
      // the cadence; 250ms is the slowest present we still report as live.
      const float sample_dt = (std::min)(dt, kMaxSampleDt);
      const float instant = 1.0f / sample_dt;
      fps = (prev > 1.0e-3f) ? (kAlpha * instant + (1.0f - kAlpha) * prev)
                             : instant;
    } else {
      fps = prev;
    }
    hud_fps_.store(fps, std::memory_order_relaxed);
  }
  // Optional rolling sample for FPS self-evolve loops (env path).
  if (const char* path = base::switch_cstr("map-fps-log")) {
    if (path[0]) {
      static auto last_write = std::chrono::steady_clock::time_point{};
      static float sum = 0.f;
      static int n = 0;
      sum += fps;
      ++n;
      const auto now = std::chrono::steady_clock::now();
      if (last_write.time_since_epoch().count() == 0 ||
          now - last_write >= std::chrono::milliseconds(500)) {
        last_write = now;
        const float mean = n > 0 ? (sum / static_cast<float>(n)) : 0.f;
        FILE* f = nullptr;
        if (fopen_s(&f, path, "a") == 0 && f) {
          std::fprintf(f, "{\"fps\":%.3f,\"mean_500ms\":%.3f,\"n\":%d}\n", fps,
                       mean, n);
          std::fflush(f);
          std::fclose(f);
        }
        sum = 0.f;
        n = 0;
      }
    }
  }
}

void DrawHost::maybe_sync_identity_hud() {
  const DWORD now = GetTickCount();
  if (last_hud_sync_tick_ == 0 || now - last_hud_sync_tick_ >= 250u) {
    last_hud_sync_tick_ = now;
    sync_identity_frame();
  }
}

void DrawHost::sync_identity_frame() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }

  const detail::EngineIdentity id = resolve_engine_identity(role_, mode_);
  const float fps = hud_fps_.load(std::memory_order_relaxed);
  wchar_t title[192] = {};
  _snwprintf_s(title, _TRUNCATE, L"%s · %s  Fps%.3f", id.role_name,
               id.engine_title, fps);
  SetWindowTextW(hwnd, title);

  // Default ON (legacy SmartGis yellow/black bar). map-identity-hud=0 hides.
  if (!show_map_identity_hud()) {
    if (identity_badge_ && IsWindow(identity_badge_)) {
      DestroyWindow(identity_badge_);
    }
    identity_badge_ = nullptr;
    identity_badge_parent_ = nullptr;
    return;
  }

  // Match leftover SmartGis.exe: black top bar + yellow "id | Engine  Fps".
  wchar_t hud[220] = {};
  _snwprintf_s(hud, _TRUNCATE, L"%s | %s  Fps%.3f", id.engine_id,
               id.engine_title, fps);

  HWND surface = hwnd;
  if (gpu_present_hwnd_ && IsWindow(gpu_present_hwnd_) &&
      IsWindowVisible(gpu_present_hwnd_)) {
    surface = gpu_present_hwnd_;
  }
  RECT parent_rc = {};
  GetClientRect(surface, &parent_rc);
  const int bar_w = parent_rc.right > 0 ? parent_rc.right : 420;
  const LONG_PTR ex = GetWindowLongPtrW(surface, GWL_EXSTYLE);
  const bool overlay_hud =
      surface == gpu_present_hwnd_ &&
      (ex & WS_EX_NOREDIRECTIONBITMAP) != 0;

  register_identity_hud_class();
  if (identity_badge_ &&
      (!IsWindow(identity_badge_) || identity_badge_parent_ != surface)) {
    if (IsWindow(identity_badge_)) {
      DestroyWindow(identity_badge_);
    }
    identity_badge_ = nullptr;
    identity_badge_parent_ = nullptr;
  }
  bool just_created = false;
  if (!identity_badge_) {
    if (overlay_hud) {
      RECT wr = {};
      GetWindowRect(surface, &wr);
      identity_badge_ = CreateWindowExW(
          WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED |
              WS_EX_TRANSPARENT,
          kIdentityHudClass, hud, WS_POPUP | WS_VISIBLE, wr.left, wr.top,
          bar_w, kIdentityHudHeight, surface, nullptr,
          GetModuleHandleW(nullptr), nullptr);
      if (identity_badge_) {
        SetLayeredWindowAttributes(identity_badge_, 0, 255, LWA_ALPHA);
      }
    } else {
      identity_badge_ = CreateWindowExW(
          0, kIdentityHudClass, hud, WS_CHILD | WS_VISIBLE, 0, 0, bar_w,
          kIdentityHudHeight, surface, nullptr, GetModuleHandleW(nullptr),
          nullptr);
    }
    identity_badge_parent_ = surface;
    just_created = identity_badge_ != nullptr;
  }
  if (identity_badge_) {
    wcscpy_s(identity_hud_text_, hud);
    SetWindowLongPtrW(identity_badge_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(identity_hud_text_));
    // Sync SetWindowPos on a DXGI-owned overlay can block the UI thread in
    // NtUserSetWindowPos (Responding=False after first present). Match
    // gpu_present: SWP_ASYNCWINDOWPOS, and skip moves when geometry is stable.
    constexpr UINT kAsyncHudPos = SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS |
                                  SWP_SHOWWINDOW | SWP_NOZORDER;
    if (!just_created) {
      bool need_move = true;
      if (overlay_hud) {
        RECT wr = {};
        GetWindowRect(surface, &wr);
        RECT cur = {};
        GetWindowRect(identity_badge_, &cur);
        if (cur.left == wr.left && cur.top == wr.top &&
            (cur.right - cur.left) == bar_w &&
            (cur.bottom - cur.top) == kIdentityHudHeight) {
          need_move = false;
        }
        if (need_move) {
          SetWindowPos(identity_badge_, nullptr, wr.left, wr.top, bar_w,
                       kIdentityHudHeight, kAsyncHudPos);
        }
      } else {
        RECT cur = {};
        GetWindowRect(identity_badge_, &cur);
        POINT tl{cur.left, cur.top};
        ScreenToClient(surface, &tl);
        if (tl.x == 0 && tl.y == 0 && (cur.right - cur.left) == bar_w &&
            (cur.bottom - cur.top) == kIdentityHudHeight) {
          need_move = false;
        }
        if (need_move) {
          SetWindowPos(identity_badge_, nullptr, 0, 0, bar_w,
                       kIdentityHudHeight, kAsyncHudPos);
        }
      }
    }
    InvalidateRect(identity_badge_, nullptr, FALSE);
  }
  sync_eagle_eye();
}

}  // namespace views
}  // namespace ui
