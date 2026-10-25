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
    const float dt = hud_fps_timer_.get_elapsed();
    constexpr float kMaxSampleDt = 0.25f;
    constexpr float kAlpha = 0.25f;
    const float prev = hud_fps_.load(std::memory_order_relaxed);
    if (dt > 1.0e-4f) {
      const float sample_dt = (std::min)(dt, kMaxSampleDt);
      const float instant = 1.0f / sample_dt;
      fps = (prev > 1.0e-3f) ? (kAlpha * instant + (1.0f - kAlpha) * prev)
                             : instant;
    } else {
      fps = prev;
    }
    hud_fps_.store(fps, std::memory_order_relaxed);
  }
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
  const float fps_hud = std::round(fps * 10.f) / 10.f;
  _snwprintf_s(title, _TRUNCATE, L"%s · %s  Fps%.1f", id.role_name,
               id.engine_title, fps_hud);
  SetWindowTextW(hwnd, title);

  if (!show_map_identity_hud()) {
    identity_popup_.destroy();
    return;
  }

  wchar_t hud[220] = {};
  _snwprintf_s(hud, _TRUNCATE, L"%s | %s  Fps%.1f", id.engine_id,
               id.engine_title, fps_hud);

  HWND surface = hwnd;
  if (gpu_present_hwnd_ && IsWindow(gpu_present_hwnd_) &&
      IsWindowVisible(gpu_present_hwnd_)) {
    surface = gpu_present_hwnd_;
  }
  RECT parent_rc = {};
  GetClientRect(surface, &parent_rc);
  const int bar_w = parent_rc.right > 0 ? parent_rc.right : 420;

  register_identity_hud_class();
  const bool text_changed = wcscmp(identity_hud_text_, hud) != 0;
  if (text_changed) {
    wcscpy_s(identity_hud_text_, hud);
  }

  LayeredPopupHost::Options opt;
  opt.class_name = kIdentityHudClass;
  opt.title = hud;
  opt.alpha = 255;
  opt.hit_transparent = true;
  const bool geom_dirty =
      identity_popup_.sync(surface, 0, 0, bar_w, kIdentityHudHeight, opt,
                           nullptr);
  if (identity_popup_.is_alive()) {
    SetWindowLongPtrW(identity_popup_.hwnd(), GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(identity_hud_text_));
    if (geom_dirty || text_changed) {
      identity_popup_.request_paint();
    }
  }
  sync_eagle_eye();
}

}  // namespace views
}  // namespace ui
