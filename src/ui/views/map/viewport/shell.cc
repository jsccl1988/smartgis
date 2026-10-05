// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Shell overlay staging + on-client identity HUD chrome for DrawHost.

#include "ui/views/map/viewport/draw_host.h"

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
#include "base/process/switches.h"
#include "render/rhi/rhi.h"
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
using detail::kIdentityHudClass;
using detail::kIdentityHudHeight;
using detail::register_identity_hud_class;
using detail::route_view_host_input;
using detail::route_view_host_pointer;

void DrawHost::clear_shell_overlay() {
  std::lock_guard<std::mutex> lock(shell_mu_);
  shell_bgra_.clear();
  shell_width_px_ = 0;
  shell_height_px_ = 0;
  shell_stride_bytes_ = 0;
  shell_generation_.fetch_add(1, std::memory_order_acq_rel);
}

void DrawHost::commit_shell_overlay(const uint8_t* bgra, uint32_t width_px,
                                       uint32_t height_px,
                                       uint32_t stride_bytes,
                                       uint64_t generation,
                                       uint32_t hole_clear_argb,
                                       uint32_t hole_clear_argb_alt) {
  // Copy immediately; ShellRaster pointers must not outlive the caller.
  // GPU present folds this into DrawRequest.shell via snapshot + overlay.
  if (!bgra || width_px == 0 || height_px == 0) {
    return;
  }
  const uint32_t stride =
      stride_bytes != 0 ? stride_bytes : width_px * 4u;
  if (stride < width_px * 4u) {
    return;
  }
  bool changed = false;
  LARGE_INTEGER t0 = {};
  QueryPerformanceCounter(&t0);
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
    ui::gfx::note_overlay_copy_bytes(
        static_cast<std::uint64_t>(width_px) * height_px * 4u);
    // Native draw HWNDs are skipped in shell paint; parents still bleed opaque
    // panel/shell fills into the HWND rect. Src-over of those fills on GPU present
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
  if (changed) {
    LARGE_INTEGER t1 = {};
    QueryPerformanceCounter(&t1);
    if (t1.QuadPart > t0.QuadPart) {
      ui::gfx::note_overlay_commit_qpc(
          static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
    }
    // Map-region shell changed: wake BeginFrame so HUD lands in the next GPU
    // present. Shell-only dirty must be filtered by the caller (BrowserView).
    request_frame();
  }
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
  const bool scenic_map2d = []() {
    const char* map_eng = base::switch_cstr("map2d-engine");
    if (map_eng && map_eng[0] && _stricmp(map_eng, "scenic") == 0) {
      return true;
    }
    char env[32] = {};
    const DWORD n = GetEnvironmentVariableA("MAP2D_ENGINE", env, sizeof(env));
    return n > 0 && n < sizeof(env) && _stricmp(env, "scenic") == 0;
  }();
  if (role_ == Role::kScene3d) {
#if defined(HAS_SCENE3D_ENGINE)
    if (content::prefer_scene3d_scenic()) {
      wcscpy_s(engine_id, L"views-scene3d-scenic");
      wcscpy_s(engine, L"Views Scene3D (Scenic)");
    } else if (content::prefer_scene3d_flycube() && mode_ == AttachMode::kGpuPresent) {
      wcscpy_s(engine_id, L"views-scene3d-dx12");
      wcscpy_s(engine, L"Views Scene3D (GPU/DX12)");
    } else if (content::prefer_scene3d_stereo_gl()) {
      // Default leftover stereo is D3D11; OpenGL is opt-in.
      bool d3d = true;
      if (const char* api = base::switch_cstr("stereo-api")) {
        if (_stricmp(api, "OpenGL") == 0) {
          d3d = false;
        } else if (_stricmp(api, "Direct3D") == 0) {
          d3d = true;
        }
      } else if (const char* flag = base::switch_cstr("scene3d-showcase-d3d")) {
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
      if (mode_ == AttachMode::kGpuPresent) {
        wcscpy_s(engine_id, L"views-scene3d-dx12");
        wcscpy_s(engine, L"Views Scene3D (GPU/DX12)");
      } else if (mode_ == AttachMode::kContentMapView) {
        wcscpy_s(engine_id, L"views-scene3d-content");
        wcscpy_s(engine, L"Views Scene3D (Content)");
      } else {
        wcscpy_s(engine_id, L"views-scene3d");
        wcscpy_s(engine, L"Scene3D");
      }
    }
  } else if (scenic_map2d) {
    wcscpy_s(engine_id, L"views-map2d-scenic");
    wcscpy_s(engine, L"Views Map2D (Scenic)");
  } else if (mode_ == AttachMode::kGpuPresent) {
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

  // Default ON (legacy SmartGis yellow/black bar). map-identity-hud=0 hides.
  const bool show_hud = [] {
    const char* v = base::switch_cstr("map-identity-hud");
    if (!v || v[0] == '\0') {
      return true;
    }
    return !(v[0] == '0' && v[1] == '\0');
  }();
  if (!show_hud) {
    if (identity_badge_ && IsWindow(identity_badge_)) {
      DestroyWindow(identity_badge_);
    }
    identity_badge_ = nullptr;
    identity_badge_parent_ = nullptr;
    return;
  }

  // Match leftover SmartGis.exe: black top bar + yellow "id | Engine  Fps".
  wchar_t hud[220] = {};
  _snwprintf_s(hud, _TRUNCATE, L"%s | %s  Fps%.3f", engine_id, engine, fps);

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
  }
  if (identity_badge_) {
    wcscpy_s(identity_hud_text_, hud);
    SetWindowLongPtrW(identity_badge_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(identity_hud_text_));
    if (overlay_hud) {
      RECT wr = {};
      GetWindowRect(surface, &wr);
      SetWindowPos(identity_badge_, HWND_TOP, wr.left, wr.top, bar_w,
                   kIdentityHudHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
      SetWindowPos(identity_badge_, HWND_TOP, 0, 0, bar_w, kIdentityHudHeight,
                   SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    InvalidateRect(identity_badge_, nullptr, FALSE);
  }
}

bool DrawHost::snapshot_shell_overlay(std::vector<uint8_t>* out_bgra,
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

}  // namespace views
}  // namespace ui
