// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/horizon/sema/expect.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "app/views/il.runtime/backend/view/probe.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/document/map_scene.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/map/viewport/draw_host.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "base/process/switches.h"

namespace app {
namespace detail {

LayoutIssues collect_layout_issues(ui::views::View* root) {
  LayoutIssues out;
  out.violation_count =
      ui::views::collect_layout_violations(root, &out.violations);
  out.overlap_count = ui::views::collect_sibling_overlaps(root, &out.overlaps);
  return out;
}

void write_layout_forensics(const LayoutForensics& note,
                            const std::vector<std::string>& issues) {
  namespace fs = std::filesystem;
  const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  const char* prefix = note.run_prefix ? note.run_prefix : "harness";
  const std::string run_id = std::string(prefix) + "_" + std::to_string(stamp);
  fs::path root = fs::path("out") / "ui_forensics";
  if (note.beside_config && (fs::exists(fs::path("..") / "Debug") ||
                             fs::exists(fs::path("..") / "Release"))) {
    root = fs::path("..") / "ui_forensics";
  }
  const fs::path dir = root / run_id;
  std::error_code ec;
  fs::create_directories(dir, ec);
  ui::views::write_layout_issues_file(dir / "layout_issues.txt", issues);
  std::ofstream man(dir / "manifest.json", std::ios::binary);
  if (man) {
    const char* scenario = note.scenario ? note.scenario : "--harness";
    man << "{\n"
        << "  \"run_id\": \"" << run_id << "\",\n"
        << "  \"exe\": \"SmartGIS.exe\",\n"
        << "  \"scenario\": \"" << scenario << "\",\n"
        << "  \"issue_count\": " << issues.size();
    if (note.marks_json) {
      man << ",\n  \"marks\": " << note.marks_json << "\n";
    } else {
      man << "\n";
    }
    man << "}\n";
  }
  std::fprintf(stderr, "ui forensics: %s\n", dir.string().c_str());
}

bool layout_forensics_forced() {
  const char* v = base::switch_cstr("ui-forensics");
  return v && v[0] && !(v[0] == '0' && v[1] == '\0');
}

bool fill_shell_status(Browser& browser, content::ShellStatus* out) {
  if (!out) {
    return false;
  }
  *out = {};
  ui::views::View* root = browser.contents_view();
  if (!root) {
    return true;
  }
  out->root_present = 1;
  out->child_count = static_cast<int>(root->child_count());
  if (out->child_count > 1) {
    if (ui::views::View* columns = root->child_at(1)) {
      out->columns_present = 1;
      out->column_count = static_cast<int>(columns->child_count());
    }
  }
  if (ui::views::CatalogView* catalog = browser.catalog_view()) {
    out->catalog_present = 1;
    out->layer_tree = catalog->layer_tree() ? 1 : 0;
  }
  return true;
}

bool fill_layout_status(Browser& browser, content::LayoutStatus* out) {
  if (!out) {
    return false;
  }
  *out = {};
  const LayoutIssues layout = collect_layout_issues(browser.contents_view());
  out->violation_count = layout.violation_count;
  out->overlap_count = layout.overlap_count;
  if (out->violation_count > 0 || layout_forensics_forced()) {
    std::vector<std::string> all = layout.violations;
    all.insert(all.end(), layout.overlaps.begin(), layout.overlaps.end());
    write_layout_forensics(
        LayoutForensics{
            .run_prefix = "harness",
            .scenario = "--harness",
            .marks_json = "[\"layout-checked\"]",
        },
        all);
  }
  if (out->violation_count > 0) {
    for (const std::string& issue : layout.violations) {
      std::fprintf(stderr, "layout smoke: %s\n", issue.c_str());
    }
  }
  return true;
}

bool fill_map_load_status(Browser& browser,
                          const std::string& face,
                          int timeout_ms,
                          content::ViewLoadStatus* out) {
  if (!out) {
    return false;
  }
  *out = {};
  out->face = face;
  const bool scene = face == "scene" || face == "scene3d";
  ui::views::DrawHost* pane =
      scene ? browser.scene_draw_host() : browser.draw_host();
  if (content::MapScene* doc = browser.document()) {
    out->layer_count = static_cast<int>(doc->layer_count());
  }
  if (content::OrbitFrame* orbit = browser.orbit_frame()) {
    out->orbit_moved =
        std::fabs(orbit->yaw() - content::kScene3dDefaultYaw) >= 0.001f ? 1
                                                                        : 0;
  }
  if (!pane) {
    return true;
  }
  out->pane_present = 1;
  out->content_view =
      pane->attach_mode() == ui::views::DrawHost::AttachMode::kContentMapView
          ? 1
          : 0;
  // WaitFrameReady blocks without pumping the UI thread. ContentMapView
  // present (and shell layout) needs message dispatch — pump until the
  // viewport has a presented frame or the budget expires.
  if (timeout_ms > 0 && out->content_view) {
    if (scene) {
      pane->sync_native_bounds();
      pane->invalidate_native();
    }
    const DWORD budget = static_cast<DWORD>(timeout_ms);
    const DWORD t0 = GetTickCount();
    for (;;) {
      if (viewport_has_presented_frame(pane)) {
        break;
      }
      if (GetTickCount() - t0 >= budget) {
        (void)pane->wait_ready(1);
        break;
      }
      pump_messages(50);
    }
  }
  const ui::views::Rect& vb = pane->bounds();
  out->view_x = vb.x;
  out->view_y = vb.y;
  out->view_w = vb.width;
  out->view_h = vb.height;
  HWND hwnd = pane->native_view();
  out->hwnd_alive = hwnd && IsWindow(hwnd) ? 1 : 0;
  out->visible = out->hwnd_alive && IsWindowVisible(hwnd) ? 1 : 0;
  out->frame_ready = viewport_has_presented_frame(pane) ? 1 : 0;
  if (out->hwnd_alive && browser.hwnd()) {
    RECT wr = {};
    GetWindowRect(hwnd, &wr);
    POINT tl = {wr.left, wr.top};
    ScreenToClient(browser.hwnd(), &tl);
    out->hwnd_x = tl.x;
    out->hwnd_y = tl.y;
    out->hwnd_w = wr.right - wr.left;
    out->hwnd_h = wr.bottom - wr.top;
  }
  if (ui::views::View* root = browser.contents_view()) {
    if (ui::views::View* menu = root->child_at(0)) {
      out->menu_h = menu->bounds().height;
    }
  }
  out->menu_min_px = ui::views::dip_to_px(22, 1.f);
  return true;
}

}  // namespace detail
}  // namespace app
