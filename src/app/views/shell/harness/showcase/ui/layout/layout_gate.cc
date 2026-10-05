// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/layout/layout_gate.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/ui/session/shell_prep.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
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

void showcase_mark(const char* token) {
  write_mark(kUiShowcaseMarkLeaf, token, /*truncate=*/false);
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

ui::views::DrawHost* active_for_mode(Browser& browser, UiShowcaseMode mode) {
  if (mode == UiShowcaseMode::kData) {
    return browser.data_draw_host();
  }
  if (mode == UiShowcaseMode::kScene) {
    return browser.scene_draw_host();
  }
  return browser.draw_host();
}

ui::views::TabStrip* map_tab_strip(ui::views::DrawHost* map) {
  for (ui::views::View* v = map; v; v = v->parent()) {
    if (auto* tabs = dynamic_cast<ui::views::TabStrip*>(v)) {
      return tabs;
    }
  }
  return nullptr;
}

}  // namespace

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
  hide_inactive_map_hwnds(browser, active_for_mode(browser, mode));
  if (ui::views::Widget* w = root->widget()) {
    w->layout_contents();
  } else {
    root->layout();
  }
  root->sync_native_tree();
  ui::views::DrawHost* active = active_for_mode(browser, mode);
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
  const bool force_dump = [] {
    const char* v = base::switch_cstr("ui-forensics");
    return v && v[0] && !(v[0] == '0' && v[1] == '\0');
  }();
  if (total_fails > 0 || force_dump) {
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const std::string run_id =
        std::string("ui_showcase_") + ui_showcase_name(mode) + "_" +
        std::to_string(stamp);
    fs::path forensics_root = fs::path("out") / "ui_forensics";
    if (fs::exists(fs::path("..") / "Debug") ||
        fs::exists(fs::path("..") / "Release")) {
      forensics_root = fs::path("..") / "ui_forensics";
    }
    const fs::path dir = forensics_root / run_id;
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::vector<std::string> all = layout_issues;
    all.insert(all.end(), overlap_issues.begin(), overlap_issues.end());
    all.insert(all.end(), shell_issues.begin(), shell_issues.end());
    ui::views::write_layout_issues_file(dir / "layout_issues.txt", all);
    {
      std::ofstream man(dir / "manifest.json", std::ios::binary);
      if (man) {
        man << "{\n"
            << "  \"run_id\": \"" << run_id << "\",\n"
            << "  \"exe\": \"SmartGIS.exe\",\n"
            << "  \"scenario\": \"--ui-showcase=" << ui_showcase_name(mode)
            << "\",\n"
            << "  \"issue_count\": " << all.size() << "\n"
            << "}\n";
      }
    }
    std::fprintf(stderr, "ui forensics: %s\n", dir.string().c_str());
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

}  // namespace detail
}  // namespace app
