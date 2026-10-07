// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/horizon/sema/ui.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/il.runtime/backend/horizon/atom/capture.h"
#include "app/views/il.runtime/backend/view/host/capture_host.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "app/views/il.runtime/backend/view/present/env.h"
#include "app/views/il.runtime/backend/horizon/sema/expect.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "app/views/il.runtime/backend/plugin/dispatch.h"
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

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include <windows.h>
#include "base/process/switches.h"

namespace app {
namespace detail {

UiMode ui_mode_from_name(const std::string& mode) {
  if (mode == "shell") {
    return UiMode::kShell;
  }
  if (mode == "data") {
    return UiMode::kData;
  }
  if (mode == "scene") {
    return UiMode::kScene;
  }
  if (mode == "catalog") {
    return UiMode::kCatalog;
  }
  if (mode == "interact") {
    return UiMode::kInteract;
  }
  return UiMode::kNone;
}

const char* ui_mode_name(UiMode mode) {
  switch (mode) {
    case UiMode::kShell:
      return "shell";
    case UiMode::kData:
      return "data";
    case UiMode::kScene:
      return "scene";
    case UiMode::kCatalog:
      return "catalog";
    case UiMode::kInteract:
      return "interact";
    case UiMode::kNone:
    default:
      return "none";
  }
}

namespace {

void ui_mark(const char* token) {
  write_mark(kUiMarkLeaf, token, /*truncate=*/false);
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

ModeSpec spec_for(UiMode mode) {
  switch (mode) {
    case UiMode::kData:
      return {
          .bmp_leaf = L"ui-showcase-data.bmp",
          .perf_leaf = "ui-showcase-data-perf.json",
          .pane = PaneId::kData,
          .capture_fallback_to_map = true,
          .tab_before_capture = 0,
      };
    case UiMode::kScene:
      return {
          .bmp_leaf = L"ui-showcase-scene.bmp",
          .perf_leaf = "ui-showcase-scene-perf.json",
          .pane = PaneId::kScene,
          .tab_before_capture = 1,
          .warm_scene_present = true,
          .tab_after_layout = 1,
          .post_layout_pump_ms = 120,
      };
    case UiMode::kCatalog:
      return {
          .bmp_leaf = L"ui-showcase-catalog.bmp",
          .perf_leaf = "ui-showcase-catalog-perf.json",
          // Reassert Maps after layout (open_map / wire_catalog reset Layers).
          .post_layout_pump_ms = 120,
      };
    case UiMode::kInteract:
      return {
          .bmp_leaf = L"ui-showcase-interact.bmp",
          .perf_leaf = "ui-showcase-interact-perf.json",
          .tab_after_layout = 0,
          .post_layout_pump_ms = 200,
      };
    case UiMode::kShell:
    case UiMode::kNone:
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

// Hide Scene3d DXGI popups that are not |active|. Do not pause_present()
// (Display join deadlocks under layout). Skipping |active| avoids racing
// sync_native_bounds against a live FlyCube Display thread (ExitProcess -1).
// Capture path still hides-then-warms around force_ui_shell_repaint so Widget
// layout never runs with a visible scene present (timeout 124).
void hide_inactive_scene3d_presents(Browser& browser,
                                    ui::views::DrawHost* active) {
  auto hide = [active](ui::views::DrawHost* pane) {
    if (!pane || pane == active) {
      return;
    }
    if (pane->role() != ui::views::DrawHost::Role::kScene3d) {
      return;
    }
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

// Soft-hide only: no sync_native_bounds (avoids Display race). Used before
// shell layout/repaint while the scene tab stays selected.
void soft_hide_scene3d_presents(Browser& browser) {
  auto hide = [](ui::views::DrawHost* pane) {
    if (!pane || pane->role() != ui::views::DrawHost::Role::kScene3d) {
      return;
    }
    pane->set_gpu_present_visible(false);
    if (HWND present = pane->present_hwnd()) {
      if (IsWindow(present)) {
        ShowWindow(present, SW_HIDE);
      }
    }
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
                           UiMode mode,
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
      ui_mode_name(mode), static_cast<double>(hud_fps),
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
    browser.select_view_tab(spec.tab_after_layout);
  }
  // Catalog showcase: layout_gate / wire can snap back to Layers — pin Maps
  // (index 2) again so the capture matches apply_ui_scenario_panels
  // (visual_review #3).
  if (spec.bmp_leaf && std::wcsstr(spec.bmp_leaf, L"catalog")) {
    if (ui::views::CatalogView* cat = browser.catalog_view()) {
      if (ui::views::TabStrip* tabs = cat->source_tabs()) {
        if (tabs->tab_count() > 2) {
          tabs->set_active(2);
        }
      }
      cat->schedule_paint();
    }
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
  // Drain queued WM_TIMER as well as KillTimer (see stop_present_timers).
  stop_present_timers(browser);
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

int ui_linger_ms() {
  LingerEnvOpts opts;
  opts.timed_ms_env = "ui-showcase-timed-ms";
  opts.linger_ms_env = "ui-showcase-linger-ms";
  opts.linger_ms_zero_only = false;
  opts.default_until_close = false;
  return parse_linger_env(opts).ms;
}

void run_horizon_map2d_fps_bench(Browser& browser) {
  (void)with_harness_shell(browser, [](plugin::HarnessShell& shell) {
    plugin::detail::bind_map2d_scenario_shell(&shell);
    plugin::detail::run_optional_map2d_fps_bench(shell, shell.map2d());
    return 0;
  });
}

void apply_ui_scenario_panels(Browser& browser, UiMode mode) {
  switch (mode) {
    case UiMode::kData:
      browser.select_view_tab(0);
      pump_views_messages(350);
      break;
    case UiMode::kScene: {
      browser.select_view_tab(1);
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
          ui_mark("scene-flycube-ok");
        } else {
          ui_mark("scene-flycube-wait");
        }
        // Soft-hide before ContourSheet/TIN upload. Live FlyCube Display +
        // apply_contour_suite_defaults has ExitProcess(-1)'d under ui.scene
        // (marks stop at interact-pre). Do not pause_present (Display join
        // deadlocks layout — suite timeout 124).
        soft_hide_scene3d_presents(browser);
        apply_china_scene3d_product_defaults(browser);
        scene->set_gpu_present_visible(true);
        run_scene_ticks(*scene, SceneTick{
                                    .count = 16,
                                    .invalidate = true,
                                    .request_frame = true,
                                });
      } else {
        apply_china_scene3d_product_defaults(browser);
      }
      if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
        scene->invalidate_native();
      }
      pump_views_messages(400);
      break;
    }
    case UiMode::kCatalog:
      browser.select_view_tab(0);
      if (ui::views::CatalogView* cat = browser.catalog_view()) {
        // Maps page lists open docs (China); Layers is the default after seed.
        if (ui::views::TabStrip* tabs = cat->source_tabs()) {
          if (tabs->tab_count() > 2) {
            tabs->set_active(2);  // Maps
          }
        }
        cat->schedule_paint();
      }
      pump_views_messages(350);
      break;
    case UiMode::kInteract:
      // Language frontend runs ui.interact.il from apply_scenario_panels.
      // This path is the tab fallback when that script did not run.
      browser.select_view_tab(0);
      pump_views_messages(200);
      browser.select_view_tab(1);
      pump_views_messages(250);
      browser.select_view_tab(0);
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
    case UiMode::kShell:
    case UiMode::kNone:
    default:
      // Short settle only — long pumps after China seed can AV when a present
      // timer races shell Yoga remeasure (visual_review #1 residual).
      browser.select_view_tab(0);
      pump_views_messages(80);
      break;
  }
}

int run_ui_layout_gate(Browser& browser, UiMode mode) {
  ui::views::View* root = browser.contents_view();
  if (!root) {
    ui_mark("root-fail");
    return 4;
  }
  ui_mark("root-ok");

  // Prefer hiding inactive Scene3d DXGI before Widget layout. Do not
  // pause_present() (Display join) and do not HideWindow the *active* scene
  // present here — that races the FlyCube Display thread (ExitProcess -1).
  // Capture soft-hides around force_ui_shell_repaint instead (timeout 124).
  const ModeSpec spec = spec_for(mode);
  ui::views::DrawHost* active = host_for(browser, spec.pane, false);
  hide_inactive_scene3d_presents(browser, active);
  layout_widget(root, /*layout_if_no_widget=*/true);
  root->sync_native_tree();
  active = host_for(browser, spec.pane, false);
  hide_inactive_scene3d_presents(browser, active);
  if (HWND hwnd = browser.hwnd()) {
    InvalidateRect(hwnd, nullptr, FALSE);
  }
  pump_views_messages(80);
  hide_inactive_scene3d_presents(browser, active);

  ui::views::TabStrip* map_tabs = map_tab_strip(browser.draw_host());
  ui::views::TabStrip* catalog_tabs =
      browser.catalog_view() ? browser.catalog_view()->source_tabs() : nullptr;

  const LayoutIssues layout = collect_layout_issues(root);
  std::vector<std::string> shell_issues;
  const int shell_fails = ui::views::collect_shell_layout_anomalies(
      root, map_tabs, catalog_tabs, browser.status_bar(), active,
      browser.data_draw_host(), browser.scene_draw_host(), &shell_issues);
  ui_mark("layout-checked");

  const int total_fails =
      layout.violation_count + layout.overlap_count + shell_fails;
  if (total_fails > 0 || layout_forensics_forced()) {
    std::vector<std::string> all = layout.violations;
    all.insert(all.end(), layout.overlaps.begin(), layout.overlaps.end());
    all.insert(all.end(), shell_issues.begin(), shell_issues.end());
    const std::string prefix = std::string("ui_") + ui_mode_name(mode);
    const std::string scenario =
        std::string("ui.mode=") + ui_mode_name(mode);
    write_layout_forensics(
        LayoutForensics{
            .run_prefix = prefix.c_str(),
            .scenario = scenario.c_str(),
            .beside_config = true,
        },
        all);
  }
  if (total_fails > 0) {
    for (const std::string& issue : layout.violations) {
      std::fprintf(stderr, "ui layout: %s\n", issue.c_str());
    }
    for (const std::string& issue : layout.overlaps) {
      std::fprintf(stderr, "ui overlap: %s\n", issue.c_str());
    }
    for (const std::string& issue : shell_issues) {
      std::fprintf(stderr, "ui shell: %s\n", issue.c_str());
    }
    ui_mark("layout-fail");
    stop_ui_map_present(browser);
    return 30;
  }
  return 0;
}

int run_ui_present_capture(Browser& browser, UiMode mode) {
  const ModeSpec spec = spec_for(mode);
  const int linger = ui_linger_ms();
  if (linger > 0) {
    pump_views_messages(static_cast<DWORD>(linger));
  }

  ui::views::DrawHost* active =
      host_for(browser, spec.pane, spec.capture_fallback_to_map);
  if (spec.tab_before_capture >= 0) {
    browser.select_view_tab(spec.tab_before_capture);
  }
  // Shell layout/repaint must run with Scene3d DXGI soft-hidden. Warming
  // present first then layout_contents deadlocks (~90s → suite timeout 124).
  if (spec.warm_scene_present) {
    soft_hide_scene3d_presents(browser);
  }
  force_ui_shell_repaint(browser);
  if (spec.warm_scene_present) {
    // Stay on 3D. Capture the FlyCube present HWND (not Map / GDI hole).
    if (active) {
      warm_visible_scene(*active);
    }
    pump_views_messages(200);
  }
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
      ui_mark("hud-fps-ok");
    } else {
      ui_mark("hud-fps-bad");
    }
  } else {
    ui_mark("hud-fps-ok");
  }

  wchar_t bmp_path[MAX_PATH] = {};
  if (!exe_capture_path(bmp_path, MAX_PATH, spec.bmp_leaf)) {
    ui_mark("bmp-path-fail");
    stop_ui_map_present(browser);
    return 55;
  }
  DeleteFileW(bmp_path);
  if (HWND hwnd = browser.hwnd()) {
    ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    if (spec.tab_after_layout >= 0) {
      browser.select_view_tab(spec.tab_after_layout);
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
    std::fprintf(stderr, "ui: BMP capture failed\n");
    ui_mark("bmp-fail");
    dump_ui_paint_profile(spec, mode, hud_fps);
    stop_ui_map_present(browser);
    return 54;
  }
  ui_mark("bmp-ok");
  dump_ui_paint_profile(spec, mode, hud_fps);
  ui_mark("pass");
  stop_ui_map_present(browser);
  return 0;
}

}  // namespace detail
}  // namespace app
