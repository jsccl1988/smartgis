// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/browse/browse_showcase.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/common/capture/bmp.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/io/sample.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/self_test/probe.h"
#include "app/views/shell/harness/showcase/plugin/seed/world3d_seed.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "ui/views/map/map_viewport.h"

#include <cstdio>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace {

// Suite id for Interact IL: SMT_HARNESS_SUITE, else browse.3d when the
// SMT_UI_INTERACT_SCRIPT leaf names it, else "browse".
const char* resolve_browse_suite_id() {
  if (const char* env = std::getenv("SMT_HARNESS_SUITE")) {
    if (env[0]) {
      return env;
    }
  }
  if (const char* script = std::getenv("SMT_UI_INTERACT_SCRIPT")) {
    if (std::strstr(script, "browse.3d")) {
      return "browse.3d";
    }
  }
  return "browse";
}

bool is_browse_3d_suite(const char* suite_id) {
  return suite_id && std::strcmp(suite_id, "browse.3d") == 0;
}

const wchar_t* browse_mark_leaf(bool suite_3d) {
  return suite_3d ? detail::kBrowse3dMarkLeaf : detail::kSelfTestMarkLeaf;
}

void browse_mark(const wchar_t* leaf, const char* step) {
  detail::write_mark(leaf, step, /*truncate=*/true);
}

void browse_viewport_size(ui::views::MapViewport* pane, int* vw, int* vh) {
  *vw = 800;
  *vh = 600;
  if (!pane) {
    return;
  }
  if (HWND hwnd = pane->native_view()) {
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    if (rc.right > 32) {
      *vw = rc.right;
    }
    if (rc.bottom > 32) {
      *vh = rc.bottom;
    }
  }
}

// SMT_SKIP_AMBOX_CATALOG (set for browse-showcase) forces demo-only SeedDocument
// and skips Browser::show deferred China. Without an explicit china_city open,
// MapFrame paints a cream AABB + ocean clear (unique≈2, no roads/rivers).
// Same opener as map2d.china / ui china_seed; do this BEFORE browse.il stress
// so capture never reopens OGR after KillTimer (hang / rc=124).
bool ensure_browse_china_map(Browser& browser, const wchar_t* mark_leaf) {
  if (!browser.document()) {
    browse_mark(mark_leaf, "china-seed-nodoc");
    return false;
  }
  // Skip O(n×m) land-clip on the UI thread (product deferred-seed path).
  _putenv_s("SMT_SKIP_CHINA_LAND_CLIP", "1");
  // Hillshade bake during first china layout has hung / AVd export under
  // browse FlyCube; map2d.china scores land without requiring shade.
  _putenv_s("SMT_MAP2D_NO_HILLSHADE", "1");

  bool ok = browser.document()->has_china_extent() &&
            browser.document()->feature_count() >= 200;
  if (!ok) {
    browser.document()->seed_default(/*allow_china_bootstrap=*/true);
    ok = browser.document()->has_china_extent() &&
         browser.document()->feature_count() >= 200;
  }
  if (!ok) {
    ok = detail::try_open_china_sample(browser,
                                       /*write_stub_if_missing=*/false);
    ok = ok && browser.document()->has_china_extent() &&
         browser.document()->feature_count() >= 3;
  }
  _putenv_s("SMT_SKIP_CHINA_LAND_CLIP", "");

  if (!ok) {
    browse_mark(mark_leaf, "china-seed-miss");
    std::fprintf(stderr,
                 "browse-showcase: china sample open failed (features=%zu)\n",
                 browser.document() ? browser.document()->feature_count()
                                    : 0u);
    return false;
  }
  ensure_china_maplibre_carto(browser);
  // init/show skipped fit under SMT_SKIP_AMBOX_CATALOG.
  browser.fit_map_extent();
  browse_mark(mark_leaf, "china-seed-ok");
  std::fprintf(stderr, "browse-showcase: china seeded features=%zu\n",
               browser.document()->feature_count());
  return true;
}

// Software carto export — HWND/FlyCube BitBlt after browse.il stop_map_timers
// lands a two-tone chrome+admin_gray hollow that soft map2d_china misreads as
// land+water. Frame ViewFrame to export pixels (map2d framing.cc): client-sized
// framing + smaller export samples a cream AABB without roads.
bool capture_browse_2d_export(Browser& browser, int vw, int vh,
                              const wchar_t* mark_leaf) {
  content::Map2dPresenter* map2d = browser.map2d();
  if (!map2d) {
    return false;
  }
  browse_mark(mark_leaf, "bmp-export-begin");
  // Frame at export size only — do NOT call apply_china_map2d_product_defaults
  // / push_shared_extent after browse.il stop_map_timers + china_city stress:
  // that path ExitProcess(-1) between bmp-export-begin and bmp-framed.
  // Carto clear + china open already happened in ensure_browse_china_map.
  content::MapScene* doc = browser.document();
  if (content::ViewFrame* frame = browser.view_frame()) {
    if (doc && doc->has_china_extent()) {
      frame->apply_world_extent(content::kChinaMap2dFrameExtent, vw, vh);
    } else if (doc) {
      frame->fit_extent(*doc, vw, vh);
    }
  }
  browse_mark(mark_leaf, "bmp-framed");
  char bmp_a[MAX_PATH] = {};
  if (!detail::exe_capture_path_a(bmp_a, MAX_PATH, "browse-showcase-2d.bmp")) {
    return false;
  }
  wchar_t bmp_w[MAX_PATH] = {};
  if (!detail::exe_capture_path(bmp_w, MAX_PATH, L"browse-showcase-2d.bmp")) {
    return false;
  }
  DeleteFileW(bmp_w);
  // Do not invalidate_frame_cache here: after browse.il stop_map_timers +
  // china_city stress, gpu_.invalidate_frame_cache ExitProcess(-1) between
  // bmp-framed and bmp-export-paint. export_bmp clears the software present
  // cache and rebuilds MapFrame from the ViewFrame camera key.
  browse_mark(mark_leaf, "bmp-export-paint");
  bool painted = false;
  try {
    painted = map2d->export_bmp(bmp_a, vw, vh);
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "browse-showcase: export_bmp exception: %s\n",
                 ex.what());
    return false;
  } catch (...) {
    std::fprintf(stderr, "browse-showcase: export_bmp unknown exception\n");
    return false;
  }
  if (!painted) {
    std::fprintf(stderr, "browse-showcase: map2d export_bmp failed\n");
    return false;
  }
  browse_mark(mark_leaf, "bmp-export-wrote");
  int bw = 0;
  int bh = 0;
  detail::BmpFileCheckOpts check;
  check.allow_32bpp = true;
  check.require_color_diversity = true;
  if (!detail::bmp_file_has_visible_signal_a(bmp_a, &bw, &bh, check)) {
    std::fprintf(stderr,
                 "browse-showcase: export BMP lacks signal (%dx%d)\n", bw, bh);
    return false;
  }
  std::fprintf(stderr, "browse-showcase: export_bmp ok (%dx%d features=%zu)\n",
               bw, bh, doc ? doc->feature_count() : 0u);
  browse_mark(mark_leaf, "bmp-ok");
  return true;
}

// Kick FlyCube presents until last_gpu_present_ok or budget expires. A second
// select_map_tab(2) after browse.3d.il remounts DXGI on navy clear; linger
// must wait for DEM/atmosphere present_gpu before BitBlt.
void linger_flycube_presents(ui::views::MapViewport* pane, DWORD budget_ms) {
  if (!pane) {
    return;
  }
  const DWORD t0 = GetTickCount();
  int kicks = 0;
  while (GetTickCount() - t0 < budget_ms) {
    pane->set_flycube_present_visible(true);
    pane->resume_present_timer();
    pane->invalidate_native();
    detail::pump_messages(80);
    ++kicks;
    if (pane->last_gpu_present_ok() && kicks >= 6) {
      return;
    }
  }
}

// Software/scenic paint SoT for browse.3d (mirrors plugin try_software_paint_bmp).
// FlyCube HWND BitBlt often lands neon RGB(20,255,0) clears that false-PASS
// green_land gates; Scene3dPresenter::export_bmp needs scenic engine only.
bool capture_browse_3d_export(Browser& browser) {
  content::Scene3dPresenter* cam = browser.scene3d();
  if (!cam) {
    return false;
  }
  wchar_t bmp_w[MAX_PATH] = {};
  if (!detail::exe_capture_path(bmp_w, MAX_PATH, L"browse-showcase-3d.bmp")) {
    return false;
  }
  DeleteFileW(bmp_w);
  constexpr int kExportW = 640;
  constexpr int kExportH = 480;
  // Hypsometric filled DEM is the browse.3d SoT. Forced wireframe washed the
  // inspect frame into green edges on black and hid land fills / sky clear.
  cam->gpu().set_wireframe_enabled(false);

  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  HDC mem = CreateCompatibleDC(screen);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = kExportW;
  bmi.bmiHeader.biHeight = -kExportH;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!mem || !dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    if (mem) {
      DeleteDC(mem);
    }
    ReleaseDC(nullptr, screen);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  bool painted = false;
  try {
    cam->paint(mem, kExportW, kExportH, true);
    painted = true;
  } catch (...) {
    painted = false;
  }
  SelectObject(mem, old);

  bool wrote = false;
  if (painted) {
    const DWORD image_bytes =
        static_cast<DWORD>(kExportW * 4) * static_cast<DWORD>(kExportH);
    BITMAPFILEHEADER bfh = {};
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + image_bytes;
    BITMAPINFOHEADER bih = bmi.bmiHeader;
    bih.biSizeImage = image_bytes;
    FILE* f = nullptr;
    if (_wfopen_s(&f, bmp_w, L"wb") == 0 && f) {
      wrote = std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
              std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih) &&
              std::fwrite(bits, 1, image_bytes, f) == image_bytes;
      std::fclose(f);
    }
  }
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);

  if (!wrote) {
    std::fprintf(stderr, "browse-showcase: scene3d software paint BMP failed\n");
    return false;
  }
  int bw = 0;
  int bh = 0;
  detail::BmpFileCheckOpts check;
  check.allow_32bpp = true;
  check.require_color_diversity = true;
  if (!detail::bmp_file_has_visible_signal(bmp_w, &bw, &bh, check)) {
    std::fprintf(stderr,
                 "browse-showcase: 3d paint BMP lacks signal (%dx%d)\n", bw,
                 bh);
    return false;
  }
  std::fprintf(stderr, "browse-showcase: scene3d software paint ok (%dx%d)\n",
               bw, bh);
  browse_mark(detail::kBrowse3dMarkLeaf, "bmp3d-ok");
  return true;
}

bool capture_browse_hwnd_bmp(Browser& browser,
                             ui::views::MapViewport* pane,
                             bool is_3d) {
  HWND hwnd = nullptr;
  if (pane) {
    hwnd = pane->input_hwnd();
  }
  if (!hwnd || !IsWindow(hwnd)) {
    hwnd = browser.hwnd();
  }
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  wchar_t path[MAX_PATH] = {};
  const wchar_t* leaf =
      is_3d ? L"browse-showcase-3d.bmp" : L"browse-showcase-2d.bmp";
  if (!detail::exe_capture_path(path, MAX_PATH, leaf)) {
    return false;
  }
  detail::CaptureOpts opts;
  opts.max_attempts = is_3d ? 12 : 6;
  opts.pump_base_ms = 100;
  opts.pump_step_ms = 60;
  opts.require_shell_diversity = false;
  opts.visible = detail::VisiblePolicy::kGridLitFraction;
  if (!detail::capture_hwnd_bmp(hwnd, path, opts)) {
    return false;
  }
  if (is_3d) {
    int bw = 0;
    int bh = 0;
    detail::BmpFileCheckOpts check;
    check.require_color_diversity = true;
    if (!detail::bmp_file_has_visible_signal(path, &bw, &bh, check)) {
      return false;
    }
  }
  browse_mark(is_3d ? detail::kBrowse3dMarkLeaf : detail::kSelfTestMarkLeaf,
              is_3d ? "bmp3d-ok" : "bmp-ok");
  return true;
}

// Prefer FlyCube present HWND (input_hwnd): shell PrintWindow cannot sample
// WS_EX_NOREDIRECTIONBITMAP DXGI flip contents (hollow navy / sheared chrome).
// 2D prefers Map2dPresenter::export_bmp (same SoT as map2d.china).
void capture_browse_shell_bmp(Browser& browser, bool is_3d) {
  ui::views::MapViewport* pane =
      is_3d ? browser.map_scene_viewport() : browser.map_viewport();

  int vw = 800;
  int vh = 600;
  browse_viewport_size(pane, &vw, &vh);

  if (is_3d) {
    // browse.3d.il already selected tab 2. Do not re-select (FlyCube remount
    // race). Prefer scenic/software export SoT — FlyCube HWND BitBlt has
    // landed neon RGB(20,255,0) clears that false-PASS plugin_scene3d greens.
    detail::resume_map_present_timers(browser);
    content::Scene3dPresenter* cam = browser.scene3d();
    if (cam) {
      detail::seed_world3d_earth_atmosphere(browser, cam);
    } else {
      apply_china_scene3d_product_defaults(browser);
    }
    if (capture_browse_3d_export(browser)) {
      return;
    }
    browse_mark(detail::kBrowse3dMarkLeaf, "bmp3d-export-fail");
    if (pane) {
      pane->set_flycube_present_visible(true);
      pane->invalidate_native();
    }
    linger_flycube_presents(pane, 4500);
    (void)capture_browse_hwnd_bmp(browser, pane, true);
    return;
  }

  // Prefer software export while present may still be live. Stopping timers
  // after china_city stress has hung the UI thread waiting on Display (rc=124
  // / suite timeout). Map2d showcase also exports without KillTimer first.
  constexpr int kExportW = 640;
  constexpr int kExportH = 480;
  if (capture_browse_2d_export(browser, kExportW, kExportH,
                               detail::kSelfTestMarkLeaf)) {
    return;
  }
  browse_mark(detail::kSelfTestMarkLeaf, "bmp-export-fail");
}

}  // namespace

int run_browse_showcase(Browser& browser) {
  const char* suite_id = resolve_browse_suite_id();
  const bool suite_3d = is_browse_3d_suite(suite_id);
  const wchar_t* mark_leaf = browse_mark_leaf(suite_3d);
  detail::clear_mark(mark_leaf);
  detail::pump_messages(300);
  if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
    detail::detach_maps(browser);
    return 2;
  }
  browse_mark(mark_leaf, "show");
  browse_mark(mark_leaf, "hwnd-ok");

  bool browse_2d_bmp_ok = false;
  // browse.3d.il selects Map tab 2 itself; forcing tab 0 first races FlyCube
  // attach / abandon under dual-pane and has exited -1 with empty marks.
  if (!suite_3d) {
    browser.select_map_tab(0);
    detail::pump_messages(400);
    // Open china AFTER the 2D tab is active. Seeding before select_map_tab(0)
    // ExitProcess(-1) under FlyCube (marks stopped at china-seed-ok).
    (void)ensure_browse_china_map(browser, mark_leaf);
    detail::pump_messages(200);
    // Pause FlyCube present before software export — concurrent GPU present +
    // MapFrame rebuild under china_city ExitProcess(-1) mid export_bmp.
    detail::stop_map_present_timers(browser);
    constexpr int kExportW = 640;
    constexpr int kExportH = 480;
    browse_2d_bmp_ok =
        capture_browse_2d_export(browser, kExportW, kExportH, mark_leaf);
    // Resume present so browse.il pan/stress UpdateWindow + HWND record /
    // motion_gate see ContentMapView + FORCE_GDI carto (not a dead dark hole).
    detail::resume_map_present_timers(browser);
    detail::pump_messages(100);
  } else {
    browse_mark(mark_leaf, "suite-browse3d");
    detail::pump_messages(200);
    // 3D needs china land rings for atmosphere sea-mask (demo rings → dark
    // floating mesh + translucent ocean plane over DEM).
    (void)ensure_browse_china_map(browser, mark_leaf);
    detail::pump_messages(200);
  }

  if (try_run_suite_script(browser, suite_id, mark_leaf,
                           /*clear_marks=*/false)) {
    if (suite_3d) {
      // Capture while present threads still paint — stop_map_present_timers
      // first yields a hollow navy FlyCube frame (ui_shell_dark accent=0).
      detail::pump_messages(150);
      capture_browse_shell_bmp(browser, true);
    }
    // After browse.il: do NOT pump the UI queue. DispatchMessage can re-enter
    // china MapFrame / ContentMapView paint and hang past harness timeout
    // (rc=124) even though pan-ok/browse-ok/dsl-done already landed.
    browse_mark(mark_leaf, "browse-exit");
    detail::stop_map_present_timers(browser);
    return 0;
  }

  // Script failed mid-way (incomplete marks). Keep maps attached for the C++
  // navigate fallback so pan/browse/wheel marks can still land; detach after.
  // browse.3d has no 2D navigate fallback that produces browse3d-* marks.
  if (suite_3d) {
    browse_mark(mark_leaf, "dsl-fail");
    detail::pump_messages(100);
    capture_browse_shell_bmp(browser, true);
    detail::stop_map_present_timers(browser);
    return 3;
  }
  browse_mark(mark_leaf, "dsl-fallback");
  const int rc = detail::self_test_navigate(browser);
  detail::pump_messages(100);
  // 2D BMP already written pre-stress when china seeded; avoid post-stress
  // export AV. Re-try only if pre-stress export missed.
  if (!browse_2d_bmp_ok) {
    constexpr int kExportW = 640;
    constexpr int kExportH = 480;
    (void)capture_browse_2d_export(browser, kExportW, kExportH, mark_leaf);
  }
  detail::stop_map_present_timers(browser);
  return rc;
}

}  // namespace app
