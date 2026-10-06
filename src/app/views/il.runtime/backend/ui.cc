// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/ui.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/il.runtime/backend/bmp.h"
#include "app/views/il.runtime/backend/capture_host.h"
#include "app/views/il.runtime/backend/mark.h"
#include "app/views/il.runtime/backend/env.h"
#include "app/views/il.runtime/backend/gate.h"
#include "app/views/il.runtime/backend/pump.h"
#include "app/views/il.runtime/backend/dispatch.h"
#include "app/views/util/exe_sidecar_path.h"
#include "plugin/product/map2d/scenario/fps_bench.h"
#include "plugin/product/map2d/scenario/progress.h"
#include "plugin/runtime/host/capability/shell.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <windows.h>
#include "base/process/switches.h"

namespace app {
namespace detail {
namespace {

// Child client mapped into the shell DIB. Screen origin is the child's
// top-left in screen space (DXGI flip-model CAPTUREBLT source).
struct ChildPlace {
  int dst_x = 0;
  int dst_y = 0;
  int src_x = 0;
  int src_y = 0;
  int copy_w = 0;
  int copy_h = 0;
  int child_w = 0;
  int child_h = 0;
  POINT screen{0, 0};
};

bool place_child_in_shell(HWND shell,
                          HWND child,
                          int shell_w,
                          int shell_h,
                          int min_w,
                          int min_h,
                          ChildPlace* out) {
  if (!shell || !child || !out || !IsWindow(shell) || !IsWindow(child) ||
      shell_w < 8 || shell_h < 8) {
    return false;
  }
  RECT child_rc = {};
  if (!GetClientRect(child, &child_rc)) {
    return false;
  }
  const int cw = child_rc.right - child_rc.left;
  const int ch = child_rc.bottom - child_rc.top;
  if (cw < min_w || ch < min_h) {
    return false;
  }
  POINT origin = {0, 0};
  POINT shell_origin = {0, 0};
  if (!ClientToScreen(child, &origin) || !ClientToScreen(shell, &shell_origin)) {
    return false;
  }
  const int dst_x = origin.x - shell_origin.x;
  const int dst_y = origin.y - shell_origin.y;
  if (dst_x >= shell_w || dst_y >= shell_h) {
    return false;
  }
  const int copy_w = std::min(cw, shell_w - std::max(0, dst_x));
  const int copy_h = std::min(ch, shell_h - std::max(0, dst_y));
  if (copy_w < min_w || copy_h < min_h) {
    return false;
  }
  out->dst_x = dst_x;
  out->dst_y = dst_y;
  out->src_x = dst_x < 0 ? -dst_x : 0;
  out->src_y = dst_y < 0 ? -dst_y : 0;
  out->copy_w = copy_w;
  out->copy_h = copy_h;
  out->child_w = cw;
  out->child_h = ch;
  out->screen = origin;
  return true;
}

// Copy the HWND client via its window DC only. Never BitBlt from the desktop
// DC: overlapping Explorer / IDE windows polluted ui-showcase BMPs and made
// shell/catalog gates fail (light desktop horizon, missing tab accent).
bool blit_client_to_dib(HWND hwnd, HDC wnd_dc, HDC mem, int w, int h) {
  if (!hwnd || !wnd_dc || !mem || w < 1 || h < 1) {
    return false;
  }
  return BitBlt(mem, 0, 0, w, h, wnd_dc, 0, 0, SRCCOPY) != FALSE;
}

// PrintWindow of the shell often leaves the child DrawHost HWND as a dark
// hole. Blit the map client into the shell DIB at its client-relative origin.
bool composite_map_hwnd_into_shell(HWND shell,
                                   HWND map,
                                   HDC mem,
                                   int shell_w,
                                   int shell_h) {
  ChildPlace place;
  if (!mem || !place_child_in_shell(shell, map, shell_w, shell_h, 8, 8, &place)) {
    return false;
  }
  HDC map_dc = GetDC(map);
  if (!map_dc) {
    return false;
  }
  const int blit_x = std::max(0, place.dst_x);
  const int blit_y = std::max(0, place.dst_y);
  const BOOL gdi_ok = BitBlt(mem, blit_x, blit_y, place.copy_w, place.copy_h,
                             map_dc, place.src_x, place.src_y, SRCCOPY);
  ReleaseDC(map, map_dc);
  BOOL ok = gdi_ok;
  const bool dxgi_flip =
      (GetWindowLongPtrW(map, GWL_EXSTYLE) & WS_EX_NOREDIRECTIONBITMAP) != 0;
  // Flip-model DXGI has no GDI redirection bitmap. Copy DWM screen pixels of
  // the present client while the shell is TOPMOST (caller). Do not blit the
  // whole desktop — only this client rect.
  if (dxgi_flip) {
    HDC screen = GetDC(nullptr);
    if (screen) {
      ok = BitBlt(mem, blit_x, blit_y, place.copy_w, place.copy_h, screen,
                  place.screen.x + place.src_x, place.screen.y + place.src_y,
                  SRCCOPY | CAPTUREBLT);
      ReleaseDC(nullptr, screen);
    }
  }
  return ok != FALSE;
}

bool composite_hud_hwnd_into_shell(HWND shell,
                                   HWND hud,
                                   HDC mem,
                                   int shell_w,
                                   int shell_h) {
  ChildPlace place;
  if (!mem || !place_child_in_shell(shell, hud, shell_w, shell_h, 8, 4, &place)) {
    return false;
  }
  HDC tmp = CreateCompatibleDC(mem);
  HBITMAP tmp_bmp =
      tmp ? CreateCompatibleBitmap(mem, place.child_w, place.child_h) : nullptr;
  HGDIOBJ old = tmp_bmp ? SelectObject(tmp, tmp_bmp) : nullptr;
  BOOL printed = FALSE;
  if (tmp && tmp_bmp) {
    printed = PrintWindow(hud, tmp, PW_CLIENTONLY);
    if (!printed) {
      printed = PrintWindow(hud, tmp, PW_RENDERFULLCONTENT);
    }
    if (!printed) {
      HDC hdc = GetDC(hud);
      if (hdc) {
        printed = BitBlt(tmp, 0, 0, place.child_w, place.child_h, hdc, 0, 0,
                         SRCCOPY);
        ReleaseDC(hud, hdc);
      }
    }
    if (printed) {
      BitBlt(mem, std::max(0, place.dst_x), std::max(0, place.dst_y),
             place.copy_w, place.copy_h, tmp, place.src_x, place.src_y, SRCCOPY);
    }
  }
  if (tmp && old) {
    SelectObject(tmp, old);
  }
  if (tmp_bmp) {
    DeleteObject(tmp_bmp);
  }
  if (tmp) {
    DeleteDC(tmp);
  }
  return printed != FALSE;
}

// One PrintWindow / window-DC sample scored for visible pixels and shell
// diversity. |got| is the GetDIBits row count.
struct ShellSample {
  int got = 0;
  bool signal = false;
  bool diverse = false;
};

ShellSample sample_shell_dib(HDC mem,
                             HBITMAP bmp,
                             BITMAPINFOHEADER* bi,
                             unsigned char* pixels,
                             int stride,
                             int w,
                             int h) {
  ShellSample sample;
  sample.got = GetDIBits(mem, bmp, 0, static_cast<UINT>(h), pixels,
                         reinterpret_cast<BITMAPINFO*>(bi), DIB_RGB_COLORS);
  sample.signal =
      sample.got == h &&
      pixels_have_visible_signal(pixels, stride, w, h,
                                 VisiblePolicy::kSparseDistinct);
  sample.diverse =
      sample.signal && bmp_has_shell_diversity(pixels, stride, w, h);
  return sample;
}

void pin_topmost(HWND hwnd, bool on) {
  if (hwnd && IsWindow(hwnd)) {
    SetWindowPos(hwnd, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  }
}

void release_shell_dib(HDC mem, HGDIOBJ old, HBITMAP bmp, HDC wnd_dc, HWND hwnd) {
  if (mem && old) {
    SelectObject(mem, old);
  }
  if (bmp) {
    DeleteObject(bmp);
  }
  if (mem) {
    DeleteDC(mem);
  }
  if (wnd_dc) {
    ReleaseDC(hwnd, wnd_dc);
  }
}

void showcase_mark(const char* token) {
  write_mark(kUiShowcaseMarkLeaf, token, /*truncate=*/false);
}

enum class PaneId { kMap, kData, kScene };

// Artifact names plus which host and tabs a mode uses. Panel scripts stay
// procedural; this is the shared policy for the layout gate and BMP capture.
struct ModeSpec {
  const wchar_t* bmp_leaf = L"ui-showcase-shell.bmp";
  const char* perf_leaf = "ui-showcase-shell-perf.json";
  PaneId pane = PaneId::kMap;
  // Data capture uses the map host when the data host is null. The layout
  // gate does not fall back — a null data host still hides every Scene3d pane.
  bool capture_fallback_to_map = false;
  // -1: leave the map tab strip alone before warmup.
  int tab_before_capture = -1;
  bool warm_scene_present = false;
  // -1: do not re-select after shell layout or before the BMP write.
  int tab_after_layout = -1;
  // 0: skip the post-layout reassert. Scene pumps 120ms, interact 200ms.
  int post_layout_pump_ms = 0;
};

ModeSpec spec_for(UiShowcaseMode mode) {
  switch (mode) {
    case UiShowcaseMode::kData:
      return {
          .bmp_leaf = L"ui-showcase-data.bmp",
          .perf_leaf = "ui-showcase-data-perf.json",
          .pane = PaneId::kData,
          .capture_fallback_to_map = true,
          .tab_before_capture = 0,
      };
    case UiShowcaseMode::kScene:
      return {
          .bmp_leaf = L"ui-showcase-scene.bmp",
          .perf_leaf = "ui-showcase-scene-perf.json",
          .pane = PaneId::kScene,
          .tab_before_capture = 1,
          .warm_scene_present = true,
          .tab_after_layout = 1,
          .post_layout_pump_ms = 120,
      };
    case UiShowcaseMode::kCatalog:
      return {
          .bmp_leaf = L"ui-showcase-catalog.bmp",
          .perf_leaf = "ui-showcase-catalog-perf.json",
      };
    case UiShowcaseMode::kInteract:
      return {
          .bmp_leaf = L"ui-showcase-interact.bmp",
          .perf_leaf = "ui-showcase-interact-perf.json",
          .tab_after_layout = 0,
          .post_layout_pump_ms = 200,
      };
    case UiShowcaseMode::kShell:
    case UiShowcaseMode::kNone:
    default:
      return {};
  }
}

ui::views::DrawHost* host_for(Browser& browser,
                              PaneId pane,
                              bool fallback_to_map) {
  ui::views::DrawHost* host = nullptr;
  switch (pane) {
    case PaneId::kData:
      host = browser.data_draw_host();
      break;
    case PaneId::kScene:
      host = browser.scene_draw_host();
      break;
    case PaneId::kMap:
      host = browser.draw_host();
      break;
  }
  if (!host && fallback_to_map) {
    host = browser.draw_host();
  }
  return host;
}

// One FlyCube present warmup. Checks run before the tick body so a host that
// is already presenting does not take an extra pump.
struct SceneTick {
  int count = 0;
  DWORD pump_ms = 50;
  bool break_if_gpu_attached = false;
  bool break_if_visible_present = false;
  bool sync_bounds = false;
  bool attach_if_none = false;
  bool invalidate = false;
  bool request_frame = false;
};

void run_scene_ticks(ui::views::DrawHost& scene, const SceneTick& tick) {
  for (int i = 0; i < tick.count; ++i) {
    if (tick.break_if_gpu_attached &&
        scene.attach_mode() == ui::views::DrawHost::AttachMode::kGpuPresent &&
        scene.last_gpu_present_ok()) {
      break;
    }
    if (tick.break_if_visible_present) {
      HWND present = scene.present_hwnd();
      if (present && IsWindowVisible(present) && scene.last_gpu_present_ok()) {
        break;
      }
    }
    if (tick.sync_bounds) {
      scene.sync_native_bounds();
    }
    if (tick.attach_if_none &&
        scene.attach_mode() == ui::views::DrawHost::AttachMode::kNone) {
      scene.attach();
    }
    if (tick.invalidate) {
      scene.invalidate_native();
    }
    if (tick.request_frame) {
      scene.request_frame();
    }
    pump_views_messages(tick.pump_ms);
  }
}

void hide_inactive_map_hwnds(Browser& browser, ui::views::DrawHost* active) {
  auto hide = [active](ui::views::DrawHost* pane) {
    if (!pane || pane == active) {
      return;
    }
    if (pane->role() != ui::views::DrawHost::Role::kScene3d) {
      return;
    }
    // Do not pause_present here — joining Display while layout runs deadlocks.
    pane->set_gpu_present_visible(false);
    if (HWND present = pane->present_hwnd()) {
      if (IsWindow(present)) {
        ShowWindow(present, SW_HIDE);
      }
    }
    pane->sync_native_bounds();
  };
  hide(browser.draw_host());
  hide(browser.data_draw_host());
  hide(browser.scene_draw_host());
}

ui::views::TabStrip* map_tab_strip(ui::views::DrawHost* map) {
  for (ui::views::View* v = map; v; v = v->parent()) {
    if (auto* tabs = dynamic_cast<ui::views::TabStrip*>(v)) {
      return tabs;
    }
  }
  return nullptr;
}

void layout_widget(ui::views::View* root, bool layout_if_no_widget) {
  if (!root) {
    return;
  }
  if (ui::views::Widget* widget = root->widget()) {
    widget->layout_contents();
  } else if (layout_if_no_widget) {
    root->layout();
  }
}

bool ui_forensics_forced() {
  const char* v = base::switch_cstr("ui-forensics");
  return v && v[0] && !(v[0] == '0' && v[1] == '\0');
}

void write_ui_forensics(UiShowcaseMode mode,
                        const std::vector<std::string>& issues) {
  namespace fs = std::filesystem;
  const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  const std::string run_id = std::string("ui_showcase_") + ui_showcase_name(mode) +
                             "_" + std::to_string(stamp);
  fs::path forensics_root = fs::path("out") / "ui_forensics";
  if (fs::exists(fs::path("..") / "Debug") ||
      fs::exists(fs::path("..") / "Release")) {
    forensics_root = fs::path("..") / "ui_forensics";
  }
  const fs::path dir = forensics_root / run_id;
  std::error_code ec;
  fs::create_directories(dir, ec);
  ui::views::write_layout_issues_file(dir / "layout_issues.txt", issues);
  std::ofstream man(dir / "manifest.json", std::ios::binary);
  if (man) {
    man << "{\n"
        << "  \"run_id\": \"" << run_id << "\",\n"
        << "  \"exe\": \"SmartGIS.exe\",\n"
        << "  \"scenario\": \"--ui-showcase=" << ui_showcase_name(mode)
        << "\",\n"
        << "  \"issue_count\": " << issues.size() << "\n"
        << "}\n";
  }
  std::fprintf(stderr, "ui forensics: %s\n", dir.string().c_str());
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
void dump_ui_paint_profile(const ModeSpec& spec,
                           UiShowcaseMode mode,
                           float hud_fps) {
  char path[MAX_PATH] = {};
  if (!exe_capture_path_a(path, MAX_PATH, spec.perf_leaf)) {
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

void warm_visible_scene(ui::views::DrawHost& scene) {
  scene.set_gpu_present_visible(true);
  scene.sync_native_bounds();
  scene.request_frame();
  scene.invalidate_native();
  run_scene_ticks(scene, SceneTick{
                             .count = 16,
                             .break_if_visible_present = true,
                             .request_frame = true,
                         });
  run_scene_ticks(scene, SceneTick{
                             .count = 16,
                             .request_frame = true,
                         });
  scene.sync_identity_frame();
}

// layout_contents / UpdateWindow can snap the strip. Scene must stay on 3D;
// interact must end on Map so the BMP does not keep a 3D accent.
void reassert_after_layout(Browser& browser,
                           ui::views::DrawHost*& active,
                           const ModeSpec& spec) {
  if (spec.post_layout_pump_ms <= 0) {
    return;
  }
  if (spec.tab_after_layout >= 0) {
    browser.select_map_tab(spec.tab_after_layout);
  }
  if (spec.warm_scene_present) {
    if (active) {
      active->set_gpu_present_visible(true);
      active->request_frame();
      active->sync_identity_frame();
      active->sync_native_bounds();
      active->invalidate_native();
    }
  } else {
    active = browser.draw_host();
  }
  pump_views_messages(static_cast<DWORD>(spec.post_layout_pump_ms));
}

}  // namespace

void apply_ui_harness_theme() {
  const char* want = base::switch_cstr("ui-theme");
  const char* id = "dark";
  if (want && want[0]) {
    if (std::strcmp(want, "light") == 0) {
      id = "light";
    } else if (std::strcmp(want, "dark") == 0) {
      id = "dark";
    }
  }
  ui::views::ThemeService::get().ensure_builtin_packs();
  ui::views::ThemeService::get().set_theme(id, /*persist_to_disk=*/false);
}

void stop_ui_map_present(Browser& browser) {
  // Drain queued WM_TIMER as well as KillTimer (see stop_map_present_timers).
  stop_map_present_timers(browser);
}

void force_ui_shell_repaint(Browser& browser) {
  if (ui::views::View* contents = browser.contents_view()) {
    if (ui::views::Widget* w = contents->widget()) {
      w->layout_contents();
    } else {
      contents->layout();
    }
  }
  if (ui::views::DrawHost* pane = browser.draw_host()) {
    pane->sync_native_bounds();
  }
  if (ui::views::DrawHost* pane = browser.data_draw_host()) {
    pane->sync_native_bounds();
  }
  if (ui::views::DrawHost* pane = browser.scene_draw_host()) {
    pane->sync_native_bounds();
  }
  if (HWND hwnd = browser.hwnd()) {
    for (int i = 0; i < 4; ++i) {
      InvalidateRect(hwnd, nullptr, TRUE);
      UpdateWindow(hwnd);
      pump_views_messages(160);
    }
  } else {
    pump_views_messages(300);
  }
}

int ui_showcase_linger_ms() {
  LingerEnvOpts opts;
  opts.timed_ms_env = "ui-showcase-timed-ms";
  opts.linger_ms_env = "ui-showcase-linger-ms";
  opts.linger_ms_zero_only = false;
  opts.default_until_close = false;
  return parse_linger_env(opts).ms;
}

void run_horizon_map2d_fps_bench(Browser& browser) {
  (void)with_harness_shell(browser, [](plugin::HarnessShell& shell) {
    plugin::detail::bind_map2d_showcase_shell(&shell);
    plugin::detail::run_optional_map2d_fps_bench(shell, shell.map2d());
    return 0;
  });
}

bool capture_ui_shell_bmp(HWND hwnd,
                          const wchar_t* filename,
                          HWND map_hwnd,
                          HWND hud_hwnd) {
  if (!hwnd || !IsWindow(hwnd) || !filename) {
    return false;
  }
  RECT rc = {};
  if (!GetClientRect(hwnd, &rc)) {
    return false;
  }
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (w < 8 || h < 8) {
    return false;
  }
  HDC wnd_dc = GetDC(hwnd);
  if (!wnd_dc) {
    return false;
  }
  HDC mem = CreateCompatibleDC(wnd_dc);
  HBITMAP bmp = CreateCompatibleBitmap(wnd_dc, w, h);
  if (!mem || !bmp) {
    release_shell_dib(mem, nullptr, bmp, wnd_dc, hwnd);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, bmp);

  BITMAPINFOHEADER bi{};
  bi.biSize = sizeof(bi);
  bi.biWidth = w;
  bi.biHeight = -h;  // top-down
  bi.biPlanes = 1;
  bi.biBitCount = 24;
  bi.biCompression = BI_RGB;
  const int stride = ((w * 3 + 3) / 4) * 4;
  std::vector<unsigned char> pixels(static_cast<size_t>(stride) *
                                    static_cast<size_t>(h));
  bool diverse = false;
  bool have_signal = false;
  std::vector<unsigned char> best_signal;
  const bool interact_leaf =
      wcsstr(filename, L"ui-showcase-interact") != nullptr;
  pin_topmost(hwnd, true);
  // Do not TOPMOST the map child for interact: it covers Skia horizon and
  // PrintWindow then writes a hollow shell (ui.interact inspect).
  if (!interact_leaf) {
    pin_topmost(map_hwnd, true);
  }
  pin_topmost(hud_hwnd, true);
  for (int attempt = 0; attempt < 8 && !diverse; ++attempt) {
    RedrawWindow(hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
    pump_views_messages(180 + static_cast<DWORD>(attempt) * 40);

    BOOL printed =
        PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT | PW_CLIENTONLY);
    if (!printed) {
      printed = PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT);
    }
    if (printed) {
      (void)composite_map_hwnd_into_shell(hwnd, map_hwnd, mem, w, h);
      (void)composite_hud_hwnd_into_shell(hwnd, hud_hwnd, mem, w, h);
    }
    ShellSample sample =
        sample_shell_dib(mem, bmp, &bi, pixels.data(), stride, w, h);
    // Prefer the latest good PrintWindow frame — the first attempt often has
    // map pixels but stale/missing tab accent after interact scripts.
    if (sample.signal) {
      best_signal = pixels;
      have_signal = true;
    }
    diverse = sample.diverse;
    if (diverse) {
      break;
    }
    // Window-DC blit only when PrintWindow produced no visible pixels.
    // Never fall back to the desktop DC (overlapping windows polluted BMPs).
    if (!sample.signal && blit_client_to_dib(hwnd, wnd_dc, mem, w, h)) {
      (void)composite_map_hwnd_into_shell(hwnd, map_hwnd, mem, w, h);
      (void)composite_hud_hwnd_into_shell(hwnd, hud_hwnd, mem, w, h);
      sample = sample_shell_dib(mem, bmp, &bi, pixels.data(), stride, w, h);
      if (sample.signal && !have_signal) {
        best_signal = pixels;
        have_signal = true;
      }
      diverse = sample.diverse;
    }
  }
  pin_topmost(hud_hwnd, false);
  pin_topmost(map_hwnd, false);
  pin_topmost(hwnd, false);
  int got = 0;
  if (!diverse && have_signal) {
    pixels = std::move(best_signal);
    got = h;
  } else if (diverse) {
    got = h;
  }

  release_shell_dib(mem, old, bmp, wnd_dc, hwnd);
  // Reject flat fills (e.g. scene PrintWindow under DXGI present) — writing
  // them as bmp-ok hid a blank ui.scene capture from the harness.
  const bool diverse_ok = bmp_has_shell_diversity(pixels.data(), stride, w, h);
  if (got != h ||
      !pixels_have_visible_signal(pixels.data(), stride, w, h,
                                 VisiblePolicy::kSparseDistinct) ||
      !diverse_ok) {
    return false;
  }

  return write_bmp_file(filename, bi, pixels.data(), pixels.size());
}

void apply_ui_scenario_panels(Browser& browser, UiShowcaseMode mode) {
  switch (mode) {
    case UiShowcaseMode::kData:
      browser.select_map_tab(0);
      pump_views_messages(350);
      break;
    case UiShowcaseMode::kScene: {
      browser.select_map_tab(1);
      pump_views_messages(800);
      // Lazy FlyCube attach needs several present ticks before HUD leaves
      // views-scene3d.gdi / Fps0 and the DEM fills the tab (not a sticker).
      if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
        run_scene_ticks(*scene, SceneTick{
                                    .count = 160,
                                    .break_if_gpu_attached = true,
                                    .sync_bounds = true,
                                    .attach_if_none = true,
                                    .invalidate = true,
                                });
        if (scene->attach_mode() ==
                ui::views::DrawHost::AttachMode::kGpuPresent &&
            scene->last_gpu_present_ok()) {
          showcase_mark("scene-flycube-ok");
          apply_china_scene3d_product_defaults(browser);
          run_scene_ticks(*scene, SceneTick{
                                      .count = 16,
                                      .request_frame = true,
                                  });
        } else {
          showcase_mark("scene-flycube-wait");
        }
      }
      apply_china_scene3d_product_defaults(browser);
      if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
        scene->invalidate_native();
      }
      pump_views_messages(400);
      break;
    }
    case UiShowcaseMode::kCatalog:
      browser.select_map_tab(0);
      if (ui::views::CatalogView* cat = browser.catalog_view()) {
        if (ui::views::TabStrip* tabs = cat->source_tabs()) {
          tabs->set_active(2);  // Maps
        }
      }
      pump_views_messages(350);
      break;
    case UiShowcaseMode::kInteract:
      // Language frontend runs ui.interact.il from apply_scenario_panels.
      // This path is the tab fallback when that script did not run.
      browser.select_map_tab(0);
      pump_views_messages(200);
      browser.select_map_tab(1);
      pump_views_messages(250);
      browser.select_map_tab(0);
      if (ui::views::CatalogView* cat = browser.catalog_view()) {
        if (ui::views::TabStrip* tabs = cat->source_tabs()) {
          tabs->set_active(0);
          pump_views_messages(120);
          tabs->set_active(1);
          pump_views_messages(120);
          tabs->set_active(0);
        }
      }
      if (browser.ui()) {
        if (ui::views::TabStrip* insp = browser.ui()->inspector_tabs()) {
          if (insp->tab_count() > 1) {
            insp->set_active(1);
            pump_views_messages(150);
            insp->set_active(0);
          }
        }
      }
      pump_views_messages(250);
      break;
    case UiShowcaseMode::kShell:
    case UiShowcaseMode::kNone:
    default:
      // Short settle only — long pumps after China seed can AV when a present
      // timer races shell Yoga remeasure (visual_review #1 residual).
      browser.select_map_tab(0);
      pump_views_messages(80);
      break;
  }
}

int run_ui_layout_gate(Browser& browser, UiShowcaseMode mode) {
  ui::views::View* root = browser.contents_view();
  if (!root) {
    showcase_mark("root-fail");
    return 4;
  }
  showcase_mark("root-ok");

  // Hide Scene3d DXGI before Widget layout — remeasure with the present
  // popup up deadlocks the UI thread (~90s, loop timeout 124). Do not
  // pause_present() (Display join); ShowWindow(SW_HIDE) on the popup.
  const ModeSpec spec = spec_for(mode);
  hide_inactive_map_hwnds(browser, host_for(browser, spec.pane, false));
  layout_widget(root, /*layout_if_no_widget=*/true);
  root->sync_native_tree();
  ui::views::DrawHost* active = host_for(browser, spec.pane, false);
  hide_inactive_map_hwnds(browser, active);
  if (HWND hwnd = browser.hwnd()) {
    InvalidateRect(hwnd, nullptr, FALSE);
  }
  pump_views_messages(80);
  hide_inactive_map_hwnds(browser, active);

  ui::views::TabStrip* map_tabs = map_tab_strip(browser.draw_host());
  ui::views::TabStrip* catalog_tabs =
      browser.catalog_view() ? browser.catalog_view()->source_tabs() : nullptr;

  std::vector<std::string> layout_issues;
  const int layout_fails =
      ui::views::collect_layout_violations(root, &layout_issues);
  std::vector<std::string> overlap_issues;
  const int overlap_fails =
      ui::views::collect_sibling_overlaps(root, &overlap_issues);
  std::vector<std::string> shell_issues;
  const int shell_fails = ui::views::collect_shell_layout_anomalies(
      root, map_tabs, catalog_tabs, browser.status_bar(), active,
      browser.data_draw_host(), browser.scene_draw_host(), &shell_issues);
  showcase_mark("layout-checked");

  const int total_fails = layout_fails + overlap_fails + shell_fails;
  if (total_fails > 0 || ui_forensics_forced()) {
    std::vector<std::string> all = layout_issues;
    all.insert(all.end(), overlap_issues.begin(), overlap_issues.end());
    all.insert(all.end(), shell_issues.begin(), shell_issues.end());
    write_ui_forensics(mode, all);
  }
  if (total_fails > 0) {
    for (const std::string& issue : layout_issues) {
      std::fprintf(stderr, "ui-showcase layout: %s\n", issue.c_str());
    }
    for (const std::string& issue : overlap_issues) {
      std::fprintf(stderr, "ui-showcase overlap: %s\n", issue.c_str());
    }
    for (const std::string& issue : shell_issues) {
      std::fprintf(stderr, "ui-showcase shell: %s\n", issue.c_str());
    }
    showcase_mark("layout-fail");
    stop_ui_map_present(browser);
    return 30;
  }
  return 0;
}

int run_ui_present_capture(Browser& browser, UiShowcaseMode mode) {
  const ModeSpec spec = spec_for(mode);
  const int linger = ui_showcase_linger_ms();
  if (linger > 0) {
    pump_views_messages(static_cast<DWORD>(linger));
  }

  ui::views::DrawHost* active =
      host_for(browser, spec.pane, spec.capture_fallback_to_map);
  if (spec.tab_before_capture >= 0) {
    browser.select_map_tab(spec.tab_before_capture);
  }
  if (spec.warm_scene_present) {
    // Stay on 3D. Capture the FlyCube present HWND (not Map / GDI hole).
    if (active) {
      warm_visible_scene(*active);
    }
    pump_views_messages(200);
    layout_widget(browser.contents_view(), /*layout_if_no_widget=*/false);
  }

  force_ui_shell_repaint(browser);
  reassert_after_layout(browser, active, spec);

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
  if (!exe_capture_path(bmp_path, MAX_PATH, spec.bmp_leaf)) {
    showcase_mark("bmp-path-fail");
    stop_ui_map_present(browser);
    return 55;
  }
  DeleteFileW(bmp_path);
  if (HWND hwnd = browser.hwnd()) {
    ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    if (spec.tab_after_layout >= 0) {
      browser.select_map_tab(spec.tab_after_layout);
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
    dump_ui_paint_profile(spec, mode, hud_fps);
    stop_ui_map_present(browser);
    return 54;
  }
  showcase_mark("bmp-ok");
  dump_ui_paint_profile(spec, mode, hud_fps);
  showcase_mark("pass");
  stop_ui_map_present(browser);
  return 0;
}

}  // namespace detail
}  // namespace app
