// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/view/browse/still.h"

#include "content/browser/glang.runtime/host/scene/browse/dispatch.h"
#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

#include <cstdio>
#include <exception>

#include "content/browser/glang.runtime/host/view/host/capture_host.h"
#include "content/browser/glang.runtime/host/horizon/atom/mark.h"
#include "content/browser/glang.runtime/host/view/shot/paint.h"
#include "content/browser/glang.runtime/host/view/shot/export.h"
#include "content/browser/glang.runtime/host/view/pixel/gate.h"
#include "content/browser/glang.runtime/host/view/dib/read.h"
#include "content/browser/glang.runtime/host/horizon/atom/pump.h"
#include "content/browser/glang.runtime/host/utils/exe_path.h"
#include "content/browser/camera/gis_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "plugin/runtime/host/capability/harness_shell.h"
#include "base/process/switches.h"
#include "plugin/product/world3d/scene/look/look.h"
#include "plugin/runtime/host/capability/shell.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
namespace detail {
namespace {

constexpr int kMap2dStillW = 1280;
constexpr int kMap2dStillH = 720;
constexpr DWORD kFlyCubeLingerMs = 4500;
constexpr wchar_t kMap2dBmpLeaf[] = L"browse-showcase-2d.bmp";
constexpr wchar_t kScene3dBmpLeaf[] = L"browse-showcase-3d.bmp";

void mark_still(const wchar_t* leaf, const char* step) {
  // Append only. truncate=true would wipe hwnd-ok / show already written by
  // Horizon IL before capture_browse_still (same leaf as suite mark_leaf).
  write_mark(leaf, step, /*truncate=*/false);
}

bool still_signal(const wchar_t* path, bool allow_32bpp, int* w, int* h) {
  BmpFileCheckOpts check;
  check.allow_32bpp = allow_32bpp;
  check.require_color_diversity = true;
  return bmp_file_has_visible_signal(path, w, h, check);
}

void frame_map2d_still(content::ShellBridge& browser, content::GisScene* doc, int w, int h) {
  content::ViewFrame* frame = browser.view_frame();
  if (!frame) {
    return;
  }
  if (doc && doc->has_land_surface()) {
    frame->apply_world_extent(content::kChinaMap2dFrameExtent, w, h);
  } else if (doc) {
    frame->fit_extent(*doc, w, h);
  }
  // Match frame_china_map2d: extent change must drop StaticReuse MapIR so DEM
  // hillshade quads and vector meshes stay registered to the same view.
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
}

void linger_flycube_presents(content::Pane* pane, DWORD budget_ms) {
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

bool capture_scene3d_software(content::ShellBridge& browser) {
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

bool capture_scene3d_hwnd(content::ShellBridge& browser, content::Pane* pane) {
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

void seed_scene3d_still(content::ShellBridge& browser) {
  resume_present_timers(browser);
  if (browser.scene3d()) {
    (void)browser.with_harness_shell([](plugin::HarnessShell& shell) {
      const char* bare_switch = base::switch_cstr("plugin-world3d-perf-bare");
      const bool bare =
          bare_switch && bare_switch[0] == '1' && bare_switch[1] == '\0';
      (void)plugin::apply_world3d_east_china_face(
          shell.scene3d(), shell.orbit_frame(), bare);
      return 0;
    });
    return;
  }
  browser.apply_china_scene3d_product_defaults();
}

}  // namespace

bool capture_browse_map2d(content::ShellBridge& browser, const wchar_t* mark_leaf) {
  if (!browser.map2d()) {
    return false;
  }
  mark_still(mark_leaf, "bmp-export-begin");
  content::GisScene* doc = browser.document();
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
  // frame_map2d_still already invalidates; keep true so a skipped frame path
  // cannot export a fit_extent StaticReuse sheet under the China ortho.
  opts.invalidate_cache = true;
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
  // Still framed ViewFrame at 1280×720 for export. Restore embed client size
  // and sync-warm MapIR while present timers are stopped (IL stop_map_timers
  // around capture) so FORCE_GDI HWND paint does not ensure_full on WM_PAINT.
  if (content::Pane* pane = browser.draw_host()) {
    RECT rc = {};
    if (HWND hwnd = pane->native_view(); hwnd && ::GetClientRect(hwnd, &rc)) {
      const int cw = rc.right > 0 ? rc.right : 0;
      const int ch = rc.bottom > 0 ? rc.bottom : 0;
      if (cw > 0 && ch > 0) {
        frame_map2d_still(browser, doc, cw, ch);
        if (content::Map2dPresenter* map2d = browser.map2d()) {
          (void)map2d->warm_frame_cache(static_cast<uint32_t>(cw),
                                        static_cast<uint32_t>(ch));
        }
      }
    }
  } else if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->force_invalidate_frame_cache();
  }
  return true;
}

void capture_browse_scene3d(content::ShellBridge& browser) {
  content::Pane* pane = browser.scene_draw_host();
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

bool capture_still_map2d(content::ShellBridge& browser, const wchar_t* mark_leaf) {
  (void)capture_browse_map2d(browser, mark_leaf);
  return true;
}

bool capture_still_scene3d(content::ShellBridge& browser, const wchar_t*) {
  capture_browse_scene3d(browser);
  return true;
}

void register_browse_still_faces() {
  register_still_face(StillFaceAdapter{"map2d", &capture_still_map2d});
  register_still_face(StillFaceAdapter{"scene3d", &capture_still_scene3d});
}

}  // namespace detail
}  // namespace content
