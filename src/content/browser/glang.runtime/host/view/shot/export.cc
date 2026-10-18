// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/view/shot/export.h"
#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

#include "content/browser/glang.runtime/host/view/pixel/bmp.h"
#include "content/browser/glang.runtime/host/view/pixel/gate.h"
#include "content/browser/glang.runtime/host/view/host/capture_host.h"
#include "content/browser/glang.runtime/host/view/present/map2d_gpu.h"
#include "content/browser/glang.runtime/host/horizon/atom/mark.h"
#include "content/browser/glang.runtime/host/utils/exe_path.h"
#include "content/browser/present/map2d/profile/map2d_phase_profile.h"
#include "base/core/log.h"
#include "base/process/switches.h"
#include "content/browser/glang.runtime/host/view/shot/paint.h"
#include "content/browser/glang.runtime/host/view/shot/scene_capture.h"
#include "content/browser/glang.runtime/host/horizon/atom/pump.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/public/plugin_host.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "render/rhi/rhi.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <windows.h>

namespace content {
namespace detail {
namespace {

// Lower an export frame token onto the document camera.
// Tokens: china_product, unit_square, document_extent, or a plugin frame name.
bool resolve_export_frame(content::ShellBridge& browser, const std::string& frame) {
  content::ViewFrame* vf = browser.view_frame();
  if (!vf) {
    return false;
  }
  auto dim = [](const char* key, int fallback, int lo, int hi) {
    const char* raw = std::getenv(key);
    if (!raw || !raw[0]) {
      return fallback;
    }
    const int v = std::atoi(raw);
    return (v >= lo && v <= hi) ? v : fallback;
  };
  const int cw = dim("MAP2D_SHOWCASE_W", kCaptureW, 320, 3840);
  const int ch = dim("MAP2D_SHOWCASE_H", kCaptureH, 240, 2160);
  if (frame == "china_product") {
    browser.ensure_china_maplibre_carto();
    browser.frame_china_map2d( cw, ch);
  } else if (frame == "unit_square") {
    constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
    vf->apply_world_extent(kUnit, cw, ch);
  } else if (frame == "document_extent") {
    double minx = 0.0;
    double miny = 0.0;
    double maxx = 0.0;
    double maxy = 0.0;
    if (browser.document() &&
        browser.document()->compute_extent(&minx, &miny, &maxx, &maxy) &&
        maxx > minx && maxy > miny) {
      // compute_extent returns map space (y = -lat). apply_world_extent expects
      // lon/lat Extent2 and converts to map internally — convert here once.
      const double lat_min = -maxy;
      const double lat_max = -miny;
      const double pad_x = std::max(0.05, (maxx - minx) * 0.15);
      const double pad_y = std::max(0.05, (lat_max - lat_min) * 0.15);
      const content::Extent2 live{minx - pad_x, lat_min - pad_y, maxx + pad_x,
                                  lat_max + pad_y};
      vf->apply_world_extent(live, cw, ch);
    } else {
      constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
      vf->apply_world_extent(kUnit, cw, ch);
    }
  } else {
    double min_lon = 0.0;
    double min_lat = 0.0;
    double max_lon = 0.0;
    double max_lat = 0.0;
    content::PluginHost* host = browser.plugin_host();
    if (!host || !host->lookup_export_frame(frame, &min_lon, &min_lat, &max_lon,
                                            &max_lat)) {
      return false;
    }
    const content::Extent2 extent{min_lon, min_lat, max_lon, max_lat};
    vf->apply_world_extent(extent, cw, ch);
  }
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  return true;
}

void force_reseed_splitters(ui::views::View* v) {
  if (!v) {
    return;
  }
  if (auto* split = dynamic_cast<ui::views::Splitter*>(v)) {
    split->reseed(/*force=*/true);
    // Only vertical docks (work|Diagnostic): horizontal Catalog|Map is
    // PrimaryFixed — enforce_primary_floor would flip policy and crush Map.
    if (!split->is_horizontal_split()) {
      split->enforce_primary_floor(2.f / 3.f);
    }
  }
  for (size_t i = 0; i < v->child_count(); ++i) {
    force_reseed_splitters(v->child_at(i));
  }
}

// Collapse Diagnostic Tools so FlyCube Init/export is not stuck on ~99px.
bool collapse_diagnostic_tools(ui::views::View* v) {
  if (!v) {
    return false;
  }
  if (auto* diag = dynamic_cast<ui::views::DiagnosticToolsPanel*>(v)) {
    if (diag->is_tools_visible()) {
      diag->set_visible_tools(false);
      return true;
    }
    return false;
  }
  bool any = false;
  for (size_t i = 0; i < v->child_count(); ++i) {
    any = collapse_diagnostic_tools(v->child_at(i)) || any;
  }
  return any;
}

// Narrow Catalog / Inspector docks so Map/3D present clears 320×240 score.
void pin_map_column_for_capture(ui::views::View* root) {
  if (!root) {
    return;
  }
  std::function<void(ui::views::View*)> walk = [&](ui::views::View* v) {
    if (!v) {
      return;
    }
    if (auto* cat = dynamic_cast<ui::views::CatalogView*>(v)) {
      cat->set_preferred_size({240, 0});
    }
    if (auto* tabs = dynamic_cast<ui::views::TabStrip*>(v)) {
      // Inspector side strip (Tools/Feature/…); keep a slim dock.
      if (tabs->tab_count() >= 4 && tabs->preferred_size().width > 200) {
        tabs->set_preferred_size({240, 0});
        if (ui::views::View* host = tabs->parent()) {
          host->set_preferred_size({240, 0});
        }
      }
    }
    for (size_t i = 0; i < v->child_count(); ++i) {
      walk(v->child_at(i));
    }
  };
  walk(root);
}

}  // namespace

bool export_scene3d_bmp(content::ShellBridge* b, const wchar_t* bmp_w) {
  if (!b || !bmp_w || !bmp_w[0]) {
    return false;
  }
  content::Scene3dPresenter* cam = b->scene3d();
  if (!cam) {
    return false;
  }
  // Product SoT is FlyCube DXGI (soft DEM / FORCE_GDI removed). After trackball
  // drag the orbit is noisy — re-frame China DEM, warm Display presents, then
  // HWND-capture. skip_ui_thread_present makes capture retry_dark_frame a
  // no-op, so warm+capture loops live here.
  DeleteFileW(bmp_w);
  {
    // Stale FlyCube sidecar must not outlive a failed export.
    std::wstring side(bmp_w);
    const size_t dot = side.find_last_of(L'.');
    if (dot != std::wstring::npos) {
      side.replace(dot, std::wstring::npos, L".engine.txt");
      DeleteFileW(side.c_str());
    }
  }
  // HWND-capture the live FlyCube present after drag. Avoid fit_scene_box /
  // atmosphere re-bind here — they take present_mu_ / abandon paths that raced
  // Display into a black clear while the record burst still saw DEM.
  content::Pane* scene = b->scene_draw_host();
  if (scene && content::prefer_scene3d_flycube()) {
    scene->set_gpu_present_visible(true);
    scene->resume_present_timer();
    if (scene->attach_mode() != content::Pane::AttachMode::kGpuPresent) {
      b->select_view_tab(1);
      pump_messages(200);
    }
    HWND present = shell_scene3d_capture_hwnd(scene);
    if (present && IsWindow(present)) {
      ShowWindow(present, SW_SHOWNOACTIVATE);
      SetWindowPos(present, HWND_TOP, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    }
    auto* device = static_cast<render::rhi::Device*>(scene->rhi_device());
    bool recovered_layout = false;
    // Collapse Output + pin Catalog/Inspector so present clears 320×240.
    ui::views::View* root = b->contents_view();
    (void)collapse_diagnostic_tools(root);
    pin_map_column_for_capture(root);
    force_reseed_splitters(root);
    scene->sync_native_bounds();
    pump_messages(100);
    // Trackball drag can leave yaw on navy/ocean clear; re-frame China DEM.
    b->apply_china_scene3d_orbit();
    scene->request_frame();
    pump_messages(120);
    for (int attempt = 0; attempt < 8; ++attempt) {
      if (scene->attach_mode() != content::Pane::AttachMode::kGpuPresent) {
        break;
      }
      const bool advanced = present_shell_scene3d_frame(scene, 500);
      pump_messages(50);
      present = shell_scene3d_capture_hwnd(scene);
      if (!present || !IsWindow(present)) {
        std::fprintf(stderr, "export_scene3d: attempt %d no present hwnd\n",
                     attempt);
        continue;
      }
      RECT prc = {};
      GetClientRect(present, &prc);
      int present_w = prc.right - prc.left;
      int present_h = prc.bottom - prc.top;
      if ((present_w < 640 || present_h < 360) && !recovered_layout) {
        recovered_layout = true;
        if (HWND shell = b->hwnd()) {
          if (IsWindow(shell)) {
            SetWindowPos(shell, nullptr, 0, 0, 1280, 800,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                             SWP_FRAMECHANGED);
          }
        }
        (void)collapse_diagnostic_tools(b->contents_view());
        force_reseed_splitters(b->contents_view());
        scene->sync_native_bounds();
        b->select_view_tab(1);
        pump_messages(200);
        (void)present_shell_scene3d_frame(scene, 800);
        present = shell_scene3d_capture_hwnd(scene);
        if (!present || !IsWindow(present)) {
          continue;
        }
        GetClientRect(present, &prc);
        present_w = prc.right - prc.left;
        present_h = prc.bottom - prc.top;
        std::fprintf(stderr,
                     "export_scene3d: layout recovery client=%dx%d\n",
                     present_w, present_h);
      }
      RECT scr = {};
      GetWindowRect(present, &scr);
      std::fprintf(stderr,
                   "export_scene3d: attempt %d advanced=%d client=%dx%d "
                   "screen=(%ld,%ld)-(%ld,%ld)\n",
                   attempt, advanced ? 1 : 0, present_w, present_h, scr.left,
                   scr.top, scr.right, scr.bottom);
      // Catalog+Inspector at HiDPI often leave Map/3D at ~314×303 after
      // interact resize — do not require 320 (false-failed DEM BitBlt).
      if (present_w < 280 || present_h < 200) {
        char size_mark[64] = {};
        std::snprintf(size_mark, sizeof(size_mark),
                      "interact-3d-size-%dx%d", present_w, present_h);
        write_mark(kUiMarkLeaf, size_mark, false);
        continue;
      }
      DeleteFileW(bmp_w);
      Scene3dHwndCaptureOpts cap;
      cap.bmp_path = bmp_w;
      cap.present_w = static_cast<uint32_t>(present_w);
      cap.present_h = static_cast<uint32_t>(present_h);
      cap.dst_w = 0;
      cap.dst_h = 0;
      cap.pre_capture_pump_ms = 120;
      cap.retry_dark_frame = true;
      cap.skip_when_null_gpu = false;
      // Display thread owns Present; UI-thread present_gpu races Flip model.
      cap.skip_ui_thread_present = true;
      cap.prefer_scenic_export = false;
      // Prefer grid-lit once min size matches the present client (see
      // bmp_check_opts). Diversity still rejects solid navy clears.
      cap.use_grid_lit_policy = true;
      cap.require_color_diversity = true;
      cap.allow_32bpp = true;
      cap.mean_min = 3.0;
      cap.mean_max = 240.0;
      cap.engine_sidecar = "FlyCube/DX12";
      cap.log_prefix = "export_scene3d";
      if (capture_scene3d_hwnd_bmp(cam, device, present, true, cap)) {
        write_mark(kUiMarkLeaf, "interact-3d-gpu-ok", false);
        return true;
      }
      write_mark(kUiMarkLeaf, "interact-3d-capture-reject", false);
    }
    DeleteFileW(bmp_w);
    write_mark(kUiMarkLeaf, "interact-3d-export-fail", false);
  }
  // Soft DEM paint was removed (scenic MemFrame only). A zeroed DIB used to
  // land as interact-3d.bmp + fake FlyCube sidecar after drag — refuse that.
  if (content::prefer_scene3d_flycube()) {
    return false;
  }
  const bool paused_present = scene != nullptr;
  if (paused_present) {
    scene->pause_present();
    pump_messages(60);
  }
  if (content::OrbitFrame* orbit = b->orbit_frame()) {
    orbit->set_distance(2.35f);
    orbit->set_pitch(0.62f);
  }
  cam->gpu().set_wireframe_enabled(false);
  cam->clear_overlay_tin_mesh();
  const bool wrote = write_software_scene3d_bmp(cam, bmp_w);
  bool ok = false;
  if (wrote) {
    int bw = 0;
    int bh = 0;
    BmpFileCheckOpts check;
    check.allow_32bpp = true;
    check.require_color_diversity = true;
    check.mean_min = 8.0;
    check.mean_max = 220.0;
    if (bmp_file_has_visible_signal(bmp_w, &bw, &bh, check)) {
      const char* painted = cam->render_engine_name();
      const char* sidecar = (painted && painted[0]) ? painted : "scenic";
      (void)write_engine_sidecar(bmp_w, sidecar);
      ok = true;
    } else {
      DeleteFileW(bmp_w);
    }
  }
  if (paused_present) {
    scene->resume_present_timer();
  }
  return ok;
}

bool export_map2d_bmp(content::ShellBridge* b,
                      const wchar_t* bmp_w,
                      const std::string& frame,
                      const Map2dExportOpts& opts) {
  if (!b || !bmp_w || !bmp_w[0]) {
    return false;
  }
  content::Map2dPresenter* map2d = b->map2d();
  if (!map2d) {
    return false;
  }
  char bmp_a[MAX_PATH] = {};
  if (WideCharToMultiByte(CP_ACP, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                          nullptr) <= 0) {
    return false;
  }
  if (opts.apply_frame &&
      !resolve_export_frame(*b, frame.empty() ? "document_extent" : frame)) {
    return false;
  }
  content::GisScene* doc = b->document();
  if (opts.require_features && (!doc || doc->feature_count() == 0)) {
    return false;
  }
  map2d->bind(doc, b->view_frame());
  if (opts.invalidate_cache) {
    map2d->invalidate_frame_cache();
  }
  if (opts.kick_paint) {
    kick_draw_host_paint(b->draw_host());
  }
  if (opts.pump_ms > 0) {
    pump_messages(static_cast<DWORD>(opts.pump_ms));
  }
  const int w = opts.width > 0 ? opts.width : kCaptureW;
  const int h = opts.height > 0 ? opts.height : kCaptureH;
  const bool showcase = wcsstr(bmp_w, L"showcase") != nullptr;
  const char* force_gdi = base::switch_cstr("force-gdi-map-overlay");
  const bool force_gdi_carto =
      force_gdi && force_gdi[0] == '1' && force_gdi[1] == '\0';
  // Live HWND client size is not the export size. Warm the showcase cache
  // after the pump so a paint at client size cannot clobber it.
  if (showcase && (!map2d->hosts_scenic_present() || force_gdi_carto)) {
    if (!map2d->warm_frame_cache(static_cast<uint32_t>(w),
                                 static_cast<uint32_t>(h))) {
      LOGGING(LOG_ERROR, "map2d-showcase: warm_frame_cache failed");
      return false;
    }
    write_mark(kMap2dMarkLeaf, "layout-warm", false);
  } else if (showcase) {
    write_mark(kMap2dMarkLeaf, "layout-warm-scenic-skip", false);
  }
  const char* reuse = base::switch_cstr("map2d-export-reuse");
  if (showcase && reuse && reuse[0] == '1' && reuse[1] == '\0') {
    write_mark(kMap2dMarkLeaf, "export-warm", false);
    char warm_a[MAX_PATH] = {};
    wchar_t warm_w[MAX_PATH] = {};
    if (exe_capture_path(warm_w, MAX_PATH, L"map2d-showcase-export-warm.bmp")) {
      if (WideCharToMultiByte(CP_ACP, 0, warm_w, -1, warm_a, MAX_PATH, nullptr,
                              nullptr) > 0) {
        (void)map2d->export_bmp(warm_a, w, h);
      }
    }
  }
  content::reset_map2d_phase_sample();
  const auto t0 = std::chrono::steady_clock::now();
  if (!map2d->export_bmp(bmp_a, w, h)) {
    if (showcase) {
      write_mark(kMap2dMarkLeaf, "bmp-fail", false);
      LOGGING(LOG_ERROR, "map2d-showcase: export_bmp failed");
    }
    return false;
  }
  if (showcase) {
    const long long export_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0)
            .count();
    const content::Map2dPhaseSample phase = content::map2d_last_phase_sample();
    LOGGING(LOG_INFO, "map2d-showcase: export_ms=%lld bmp_io_ms=%lld", export_ms,
            static_cast<long long>(phase.bmp_io_ms));
    LOGGING(LOG_INFO,
            "map2d-showcase: phase_export layout_ms=%lld hillshade_ms=%lld "
            "bmp_io_ms=%lld gpu_upload_ms=%lld gpu_present_ms=%lld",
            static_cast<long long>(phase.layout_ms),
            static_cast<long long>(phase.hillshade_ms),
            static_cast<long long>(phase.bmp_io_ms),
            static_cast<long long>(phase.gpu_upload_ms),
            static_cast<long long>(phase.gpu_present_ms));
    write_mark(kMap2dMarkLeaf, "bmp-wrote", false);
    int bw = 0;
    int bh = 0;
    BmpFileCheckOpts check;
    check.allow_32bpp = true;
    if (!bmp_file_has_visible_signal_a(bmp_a, &bw, &bh, check)) {
      LOGGING(LOG_ERROR, "map2d-showcase: BMP lacks visible signal (%dx%d)", bw,
              bh);
      write_mark(kMap2dMarkLeaf, "bmp-black", false);
      return false;
    }
    LOGGING(LOG_INFO, "map2d-showcase: wrote %s (%dx%d)", bmp_a, bw, bh);
    write_mark(kMap2dMarkLeaf, "bmp-ok", false);
    wchar_t keep_w[MAX_PATH] = {};
    if (wcscpy_s(keep_w, bmp_w) == 0) {
      wchar_t* dot = wcsrchr(keep_w, L'.');
      if (dot && _wcsicmp(dot, L".bmp") == 0 &&
          static_cast<size_t>(dot - keep_w) + 10 < MAX_PATH) {
        wcscpy_s(dot, static_cast<size_t>(MAX_PATH - (dot - keep_w)),
                 L".keep.bmp");
        if (CopyFileW(bmp_w, keep_w, FALSE)) {
          write_mark(kMap2dMarkLeaf, "bmp-keep", false);
        }
      }
    }
    run_map2d_showcase_gpu(*b, map2d, w, h);
  }
  return true;
}

}  // namespace detail
}  // namespace content
