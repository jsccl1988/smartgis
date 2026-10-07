// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/browse/still.h"

#include <cstdio>
#include <exception>

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/il.runtime/backend/view/host/capture_host.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "app/views/il.runtime/backend/view/shot/paint.h"
#include "app/views/il.runtime/backend/view/shot/export.h"
#include "app/views/il.runtime/backend/view/pixel/gate.h"
#include "app/views/il.runtime/backend/view/dib/read.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "app/views/il.runtime/backend/plugin/dispatch.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/product/world3d/scenario/seed/world3d_seed.h"
#include "plugin/runtime/host/capability/shell.h"
#include "ui/views/map/viewport/draw_host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

constexpr int kMap2dStillW = 1280;
constexpr int kMap2dStillH = 720;
constexpr DWORD kFlyCubeLingerMs = 4500;
constexpr wchar_t kMap2dBmpLeaf[] = L"browse-showcase-2d.bmp";
constexpr wchar_t kScene3dBmpLeaf[] = L"browse-showcase-3d.bmp";

void mark_still(const wchar_t* leaf, const char* step) {
  write_mark(leaf, step, /*truncate=*/true);
}

bool still_signal(const wchar_t* path, bool allow_32bpp, int* w, int* h) {
  BmpFileCheckOpts check;
  check.allow_32bpp = allow_32bpp;
  check.require_color_diversity = true;
  return bmp_file_has_visible_signal(path, w, h, check);
}

void frame_map2d_still(Browser& browser, content::MapScene* doc, int w, int h) {
  content::ViewFrame* frame = browser.view_frame();
  if (!frame) {
    return;
  }
  if (doc && doc->has_china_extent()) {
    frame->apply_world_extent(content::kChinaMap2dFrameExtent, w, h);
  } else if (doc) {
    frame->fit_extent(*doc, w, h);
  }
}

void linger_flycube_presents(ui::views::DrawHost* pane, DWORD budget_ms) {
  if (!pane) {
    return;
  }
  const DWORD t0 = GetTickCount();
  int kicks = 0;
  while (GetTickCount() - t0 < budget_ms) {
    pane->set_gpu_present_visible(true);
    pane->resume_present_timer();
    pane->invalidate_native();
    pump_messages(80);
    ++kicks;
    if (pane->last_gpu_present_ok() && kicks >= 6) {
      return;
    }
  }
}

bool capture_scene3d_software(Browser& browser) {
  content::Scene3dPresenter* cam = browser.scene3d();
  if (!cam) {
    return false;
  }
  wchar_t path[MAX_PATH] = {};
  if (!exe_capture_path(path, MAX_PATH, kScene3dBmpLeaf)) {
    return false;
  }
  DeleteFileW(path);
  cam->gpu().set_wireframe_enabled(false);
  if (content::OrbitFrame* orbit = browser.orbit_frame()) {
    orbit->set_distance(2.35f);
    orbit->set_pitch(0.62f);
  }
  if (!write_software_scene3d_bmp(cam, path)) {
    std::fprintf(stderr, "browse: scene3d software paint BMP failed\n");
    return false;
  }
  int bw = 0;
  int bh = 0;
  if (!still_signal(path, /*allow_32bpp=*/true, &bw, &bh)) {
    std::fprintf(stderr,
                 "browse: 3d paint BMP lacks signal (%dx%d)\n", bw,
                 bh);
    return false;
  }
  std::fprintf(stderr, "browse: scene3d software paint ok (%dx%d)\n",
               bw, bh);
  mark_still(kBrowse3dMarkLeaf, "bmp3d-ok");
  return true;
}

bool capture_scene3d_hwnd(Browser& browser, ui::views::DrawHost* pane) {
  HWND hwnd = pane ? pane->input_hwnd() : nullptr;
  if (!hwnd || !IsWindow(hwnd)) {
    hwnd = browser.hwnd();
  }
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  wchar_t path[MAX_PATH] = {};
  if (!exe_capture_path(path, MAX_PATH, kScene3dBmpLeaf)) {
    return false;
  }
  CaptureOpts opts;
  opts.max_attempts = 12;
  opts.pump_base_ms = 100;
  opts.pump_step_ms = 60;
  opts.require_shell_diversity = false;
  opts.visible = VisiblePolicy::kGridLitFraction;
  if (!capture_hwnd_bmp(hwnd, path, opts)) {
    return false;
  }
  int bw = 0;
  int bh = 0;
  if (!still_signal(path, /*allow_32bpp=*/false, &bw, &bh)) {
    return false;
  }
  mark_still(kBrowse3dMarkLeaf, "bmp3d-ok");
  return true;
}

void seed_scene3d_still(Browser& browser) {
  resume_map_present_timers(browser);
  if (browser.scene3d()) {
    (void)with_harness_shell(browser, [](plugin::HarnessShell& shell) {
      plugin::detail::bind_plugin_scenario_shell(&shell);
      plugin::detail::seed_world3d_earth_atmosphere(shell, shell.scene3d());
      return 0;
    });
    return;
  }
  apply_china_scene3d_product_defaults(browser);
}

}  // namespace

bool capture_browse_map2d(Browser& browser, const wchar_t* mark_leaf) {
  if (!browser.map2d()) {
    return false;
  }
  mark_still(mark_leaf, "bmp-export-begin");
  content::MapScene* doc = browser.document();
  frame_map2d_still(browser, doc, kMap2dStillW, kMap2dStillH);
  mark_still(mark_leaf, "bmp-framed");
  wchar_t path[MAX_PATH] = {};
  if (!exe_capture_path(path, MAX_PATH, kMap2dBmpLeaf)) {
    return false;
  }
  DeleteFileW(path);
  mark_still(mark_leaf, "bmp-export-paint");
  Map2dExportOpts opts;
  opts.apply_frame = false;
  opts.invalidate_cache = false;
  opts.kick_paint = false;
  opts.pump_ms = 0;
  opts.require_features = false;
  opts.width = kMap2dStillW;
  opts.height = kMap2dStillH;
  bool painted = false;
  try {
    painted = export_map2d_bmp(&browser, path, "document_extent", opts);
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "browse: export_bmp exception: %s\n",
                 ex.what());
    return false;
  } catch (...) {
    std::fprintf(stderr, "browse: export_bmp unknown exception\n");
    return false;
  }
  if (!painted) {
    std::fprintf(stderr, "browse: map2d export_bmp failed\n");
    return false;
  }
  mark_still(mark_leaf, "bmp-export-wrote");
  int bw = 0;
  int bh = 0;
  if (!still_signal(path, /*allow_32bpp=*/true, &bw, &bh)) {
    std::fprintf(stderr, "browse: export BMP lacks signal (%dx%d)\n",
                 bw, bh);
    return false;
  }
  std::fprintf(stderr, "browse: export_bmp ok (%dx%d features=%zu)\n",
               bw, bh, doc ? doc->feature_count() : 0u);
  mark_still(mark_leaf, "bmp-ok");
  return true;
}

void capture_browse_scene3d(Browser& browser) {
  ui::views::DrawHost* pane = browser.scene_draw_host();
  seed_scene3d_still(browser);
  if (capture_scene3d_software(browser)) {
    return;
  }
  mark_still(kBrowse3dMarkLeaf, "bmp3d-export-fail");
  if (pane) {
    pane->set_gpu_present_visible(true);
    pane->invalidate_native();
  }
  linger_flycube_presents(pane, kFlyCubeLingerMs);
  (void)capture_scene3d_hwnd(browser, pane);
}

}  // namespace detail
}  // namespace app
