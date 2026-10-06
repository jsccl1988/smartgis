// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/shell_expect.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/capture_host.h"
#include "app/views/il.runtime/backend/mark.h"
#include "content/browser/camera/orbit_frame.h"
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
namespace {

void dump_layout_forensics(const std::vector<std::string>& issues) {
  namespace fs = std::filesystem;
  const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  const std::string run_id = "harness_" + std::to_string(stamp);
  const fs::path dir = fs::path("out") / "ui_forensics" / run_id;
  std::error_code ec;
  fs::create_directories(dir, ec);
  ui::views::write_layout_issues_file(dir / "layout_issues.txt", issues);
  std::ofstream man(dir / "manifest.json", std::ios::binary);
  if (man) {
    man << "{\n"
        << "  \"run_id\": \"" << run_id << "\",\n"
        << "  \"exe\": \"SmartGIS.exe\",\n"
        << "  \"scenario\": \"--harness\",\n"
        << "  \"issue_count\": " << issues.size() << ",\n"
        << "  \"marks\": [\"layout-checked\"]\n"
        << "}\n";
  }
  std::fprintf(stderr, "ui forensics: %s\n", dir.string().c_str());
}

bool force_ui_forensics() {
  const char* v = base::switch_cstr("ui-forensics");
  return v && v[0] && !(v[0] == '0' && v[1] == '\0');
}

}  // namespace

int expect_shell_tree(Browser& browser) {
  ui::views::View* root = browser.contents_view();
  if (!root) {
    return 4;
  }
  if (root->child_count() < 3) {
    return 4;
  }
  ui::views::View* columns = root->child_at(1);
  if (!columns || columns->child_count() < 2) {
    return 5;
  }
  ui::views::CatalogView* catalog = browser.catalog_view();
  if (!catalog || !catalog->layer_tree()) {
    return 6;
  }
  return 0;
}

int expect_scene_visible(Browser& browser) {
  ui::views::DrawHost* scene = browser.scene_draw_host();
  if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
    std::fprintf(stderr, "harness: scene host missing after tab switch\n");
    return 9;
  }
  ui::views::DrawHost* map = browser.draw_host();
  if (map && map->native_view() && IsWindow(map->native_view()) &&
      IsWindowVisible(map->native_view())) {
    std::fprintf(stderr, "inactive map HWND still visible after 3D tab\n");
    return 36;
  }
  if (!IsWindowVisible(scene->native_view())) {
    std::fprintf(stderr, "active Scene HWND not visible\n");
    return 37;
  }
  return 0;
}

int expect_orbit_moved(Browser& browser) {
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!orbit ||
      std::fabs(orbit->yaw() - content::kScene3dDefaultYaw) < 0.001f) {
    return 25;
  }
  return 0;
}

int expect_layout_bounds(Browser& browser) {
  std::vector<std::string> layout_issues;
  const int layout_fails = ui::views::collect_layout_violations(
      browser.contents_view(), &layout_issues);
  std::vector<std::string> overlap_issues;
  ui::views::collect_sibling_overlaps(browser.contents_view(), &overlap_issues);
  if (layout_fails > 0 || force_ui_forensics()) {
    std::vector<std::string> all = layout_issues;
    all.insert(all.end(), overlap_issues.begin(), overlap_issues.end());
    dump_layout_forensics(all);
  }
  if (layout_fails > 0) {
    for (const std::string& issue : layout_issues) {
      std::fprintf(stderr, "layout smoke: %s\n", issue.c_str());
    }
    write_mark(kHarnessMarkLeaf, "layout-fail", false);
    detach_maps(browser);
    return 30;
  }
  return 0;
}

int expect_map_hwnd_sync(Browser& browser) {
  ui::views::DrawHost* map_pane = browser.draw_host();
  if (!map_pane || map_pane->bounds().width <= 0 ||
      map_pane->bounds().height <= 0) {
    detach_maps(browser);
    return 31;
  }
  HWND map_hwnd = map_pane->native_view();
  if (!map_hwnd || !IsWindow(map_hwnd)) {
    detach_maps(browser);
    return 35;
  }
  map_pane->sync_native_bounds();
  RECT wr = {};
  GetWindowRect(map_hwnd, &wr);
  POINT tl = {wr.left, wr.top};
  ScreenToClient(browser.hwnd(), &tl);
  const ui::views::Rect& vb = map_pane->bounds();
  const int tol = 2;
  if (tl.x < vb.x - tol || tl.x > vb.x + tol || tl.y < vb.y - tol ||
      tl.y > vb.y + tol) {
    std::fprintf(stderr, "map hwnd origin (%ld,%ld) vs view (%d,%d)\n", tl.x,
                 tl.y, vb.x, vb.y);
    detach_maps(browser);
    return 33;
  }
  const int hw = wr.right - wr.left;
  const int hh = wr.bottom - wr.top;
  if (hw < vb.width - tol || hw > vb.width + tol || hh < vb.height - tol ||
      hh > vb.height + tol) {
    std::fprintf(stderr, "map hwnd size %dx%d vs view %dx%d\n", hw, hh,
                 vb.width, vb.height);
    detach_maps(browser);
    return 34;
  }
  if (ui::views::View* root_view = browser.contents_view()) {
    if (ui::views::View* menu = root_view->child_at(0)) {
      if (menu->bounds().height < ui::views::dip_to_px(22, 1.f)) {
        detach_maps(browser);
        return 32;
      }
    }
  }
  return 0;
}

}  // namespace detail
}  // namespace app
