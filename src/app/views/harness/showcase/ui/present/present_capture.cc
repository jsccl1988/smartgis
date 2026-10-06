// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/showcase/ui/present/present_capture.h"

#include "app/views/app/cmdline/views_launch_options.h"
#include "app/views/browser/browser.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/self_test/self_test.h"
#include "app/views/harness/showcase/ui/capture/shell_capture.h"
#include "app/views/harness/showcase/ui/session/shell_prep.h"
#include "app/views/util/exe_sidecar_path.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/viewport/draw_host.h"

#include <cstdint>
#include <cstdio>
#include <windows.h>

namespace app {
namespace detail {
namespace {

void showcase_mark(const char* token) {
  write_mark(kUiShowcaseMarkLeaf, token, /*truncate=*/false);
}

const char* ui_showcase_perf_leaf_a(UiShowcaseMode mode) {
  switch (mode) {
    case UiShowcaseMode::kData:
      return "ui-showcase-data-perf.json";
    case UiShowcaseMode::kScene:
      return "ui-showcase-scene-perf.json";
    case UiShowcaseMode::kCatalog:
      return "ui-showcase-catalog-perf.json";
    case UiShowcaseMode::kInteract:
      return "ui-showcase-interact-perf.json";
    case UiShowcaseMode::kShell:
    case UiShowcaseMode::kNone:
    default:
      return "ui-showcase-shell-perf.json";
  }
}

double qpc_ticks_to_ms(std::uint64_t ticks) {
  LARGE_INTEGER freq = {};
  if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0) {
    return 0.0;
  }
  return 1000.0 * static_cast<double>(ticks) /
         static_cast<double>(freq.QuadPart);
}

// Sidecar JSON for harness-auto-ui-opt. QPC buckets converted to ms.
void dump_ui_paint_profile(UiShowcaseMode mode, float hud_fps) {
  char path[MAX_PATH] = {};
  if (!exe_capture_path_a(path, MAX_PATH, ui_showcase_perf_leaf_a(mode))) {
    return;
  }
  const ui::gfx::PaintCounters c = ui::gfx::paint_counters();
  FILE* pf = nullptr;
  if (fopen_s(&pf, path, "wb") != 0 || !pf) {
    return;
  }
  std::fprintf(
      pf,
      "{\"backend\":\"ui.views\",\"mode\":\"%s\",\"hud_fps\":%.3f,"
      "\"commit_count\":%llu,\"activate_count\":%llu,\"layout_count\":%llu,"
      "\"begin_frame_count\":%llu,\"paint_pixels\":%llu,\"rcpaint_area\":%llu,"
      "\"create_font\":%llu,\"create_brush\":%llu,\"canvas_ctors\":%llu,"
      "\"utf8_conversions\":%llu,\"measure_text\":%llu,"
      "\"overlay_copy_bytes\":%llu,"
      "\"commit_ms\":%.3f,\"raster_ms\":%.3f,\"present_ms\":%.3f,"
      "\"widget_paint_ms\":%.3f,\"map_paint_ms\":%.3f,"
      "\"hover_commit_ms\":%.3f,\"table_scroll_ms\":%.3f,"
      "\"overlay_commit_ms\":%.3f,"
      "\"begin_frame_to_present_ms\":%.3f,"
      "\"begin_frame_to_shell_present_ms\":%.3f}\n",
      ui_showcase_name(mode), static_cast<double>(hud_fps),
      static_cast<unsigned long long>(c.commit_count),
      static_cast<unsigned long long>(c.activate_count),
      static_cast<unsigned long long>(c.layout_count),
      static_cast<unsigned long long>(c.begin_frame_count),
      static_cast<unsigned long long>(c.paint_pixels),
      static_cast<unsigned long long>(c.rcpaint_area),
      static_cast<unsigned long long>(c.create_font),
      static_cast<unsigned long long>(c.create_brush),
      static_cast<unsigned long long>(c.canvas_ctors),
      static_cast<unsigned long long>(c.utf8_conversions),
      static_cast<unsigned long long>(c.measure_text),
      static_cast<unsigned long long>(c.overlay_copy_bytes),
      qpc_ticks_to_ms(c.commit_qpc), qpc_ticks_to_ms(c.raster_qpc),
      qpc_ticks_to_ms(c.present_qpc), qpc_ticks_to_ms(c.widget_paint_qpc),
      qpc_ticks_to_ms(c.map_paint_qpc), qpc_ticks_to_ms(c.hover_commit_qpc),
      qpc_ticks_to_ms(c.table_scroll_qpc),
      qpc_ticks_to_ms(c.overlay_commit_qpc),
      qpc_ticks_to_ms(c.begin_frame_to_present_qpc),
      qpc_ticks_to_ms(c.begin_frame_to_shell_present_qpc));
  std::fclose(pf);
}

}  // namespace

int run_ui_present_capture(Browser& browser, UiShowcaseMode mode) {
  const int linger = ui_showcase_linger_ms();
  if (linger > 0) {
    pump_views_messages(static_cast<DWORD>(linger));
  }

  ui::views::DrawHost* active = browser.draw_host();
  if (mode == UiShowcaseMode::kData) {
    active = browser.data_draw_host();
    if (!active) {
      active = browser.draw_host();
    }
    browser.select_map_tab(0);
  } else if (mode == UiShowcaseMode::kScene) {
    active = browser.scene_draw_host();
    // Stay on 3D. Capture the FlyCube present HWND (not Map / GDI hole).
    browser.select_map_tab(1);
    if (active) {
      active->set_gpu_present_visible(true);
      active->sync_native_bounds();
      active->request_frame();
      active->invalidate_native();
      for (int i = 0; i < 16; ++i) {
        if (active->present_hwnd() && IsWindowVisible(active->present_hwnd()) &&
            active->last_gpu_present_ok()) {
          break;
        }
        active->request_frame();
        pump_views_messages(50);
      }
      for (int i = 0; i < 16; ++i) {
        active->request_frame();
        pump_views_messages(50);
      }
      active->sync_identity_frame();
    }
    pump_views_messages(200);
    if (ui::views::Widget* w =
            browser.contents_view() ? browser.contents_view()->widget()
                                   : nullptr) {
      w->layout_contents();
    }
  }

  force_ui_shell_repaint(browser);
  if (mode == UiShowcaseMode::kScene) {
    // layout_contents / UpdateWindow can snap the strip back to Map.
    browser.select_map_tab(1);
    if (active) {
      active->set_gpu_present_visible(true);
      active->request_frame();
      active->sync_identity_frame();
      active->sync_native_bounds();
      active->invalidate_native();
    }
    pump_views_messages(120);
  } else if (mode == UiShowcaseMode::kInteract) {
    // Script ends on Map tab; layout_contents must not leave 3D accent.
    browser.select_map_tab(0);
    active = browser.draw_host();
    pump_views_messages(200);
  }

  // Require a live cadence (held across idle gaps). Fps0.000 after gestures
  // used to pass because the gate treated <0.5 as idle-OK.
  float hud_fps = 0.f;
  if (active) {
    for (int i = 0; i < 8; ++i) {
      active->request_frame();
      active->invalidate_native();
      pump_views_messages(40);
    }
    active->sync_identity_frame();
    hud_fps = active->hud_fps();
    if (hud_fps >= 1.f) {
      showcase_mark("hud-fps-ok");
    } else {
      showcase_mark("hud-fps-bad");
    }
  } else {
    showcase_mark("hud-fps-ok");
  }

  wchar_t bmp_path[MAX_PATH] = {};
  if (!exe_capture_path(bmp_path, MAX_PATH, ui_showcase_bmp_leaf(mode))) {
    showcase_mark("bmp-path-fail");
    stop_ui_map_present(browser);
    return 55;
  }
  DeleteFileW(bmp_path);
  if (HWND hwnd = browser.hwnd()) {
    ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    if (mode == UiShowcaseMode::kScene) {
      browser.select_map_tab(1);
    } else if (mode == UiShowcaseMode::kInteract) {
      browser.select_map_tab(0);
    }
    pump_views_messages(120);
  }
  HWND map_hwnd = nullptr;
  HWND hud_hwnd = nullptr;
  if (active) {
    HWND present = active->present_hwnd();
    if (!present || !IsWindow(present)) {
      present = active->input_hwnd();
    }
    if (present && IsWindow(present)) {
      map_hwnd = present;
    } else if (active->native_view() && IsWindow(active->native_view())) {
      map_hwnd = active->native_view();
    }
    hud_hwnd = active->identity_hud_hwnd();
    if (map_hwnd) {
      active->sync_identity_frame();
      hud_hwnd = active->identity_hud_hwnd();
      active->sync_native_bounds();
      active->invalidate_native();
      pump_views_messages(80);
    }
  }
  if (!capture_ui_shell_bmp(browser.hwnd(), bmp_path, map_hwnd, hud_hwnd)) {
    std::fprintf(stderr, "ui-showcase: BMP capture failed\n");
    showcase_mark("bmp-fail");
    dump_ui_paint_profile(mode, hud_fps);
    stop_ui_map_present(browser);
    return 54;
  }
  showcase_mark("bmp-ok");
  dump_ui_paint_profile(mode, hud_fps);
  showcase_mark("pass");
  stop_ui_map_present(browser);
  return 0;
}

}  // namespace detail
}  // namespace app
