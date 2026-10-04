// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/self_test/probe.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include <windows.h>
#include <shellapi.h>

#include "content/browser/camera/map_host_extent.h"
#include "content/app/content_main.h"
#include "content/embed/embed_sample.h"
#include "content/public/event_bus.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/view_host.h"
#include "content/renderer/renderer_main.h"
#include "gis/edit/memory_session.h"
#include "gis/carto/style/style_document.h"
#include "gis/carto/tile/tile_provider.h"
#include "gpu/gpu.h"
#include "net/http/http.h"
#include "render/rhi/rhi.h"
#include "tool/draft/draft.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/shell/dpi.h"
#include "base/trace/event/process_trace.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/view/view.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>
#include <cwctype>

namespace app {
namespace detail {

int self_test_layout_bounds(Browser& browser) {
// Layout smoke: bounds non-negative, children inside parents.
std::vector<std::string> layout_issues;
const int layout_fails =
    ui::views::collect_layout_violations(browser.contents_view(),
                                         &layout_issues);
// Sibling overlaps are advisory in the product shell (some stacks are
// intentional); L0 forensics tests assert them on synthetic trees.
std::vector<std::string> overlap_issues;
ui::views::collect_sibling_overlaps(browser.contents_view(),
                                    &overlap_issues);
self_test_mark("layout-checked");
const bool force_dump = [] {
  const char* v = std::getenv("SMT_UI_FORENSICS");
  return v && v[0] && !(v[0] == '0' && v[1] == '\0');
}();
if (layout_fails > 0 || force_dump) {
  // Mode A: dump text forensics on failure (or SMT_UI_FORENSICS=1).
  // Product binary stays free of testonly dump_ui_forensics; write the
  // layout issues + a minimal manifest (no shell PNG / view tree).
  namespace fs = std::filesystem;
  const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  const std::string run_id = "self_test_" + std::to_string(stamp);
  const fs::path dir = fs::path("out") / "ui_forensics" / run_id;
  std::error_code ec;
  fs::create_directories(dir, ec);
  std::vector<std::string> all = layout_issues;
  all.insert(all.end(), overlap_issues.begin(), overlap_issues.end());
  ui::views::write_layout_issues_file(dir / "layout_issues.txt", all);
  {
    std::ofstream man(dir / "manifest.json", std::ios::binary);
    if (man) {
      man << "{\n"
          << "  \"run_id\": \"" << run_id << "\",\n"
          << "  \"exe\": \"SmartGisViews.exe\",\n"
          << "  \"scenario\": \"--self-test\",\n"
          << "  \"issue_count\": " << all.size() << ",\n"
          << "  \"marks\": [\"layout-checked\"]\n"
          << "}\n";
    }
  }
  std::fprintf(stderr, "ui forensics: %s\n", dir.string().c_str());
}
if (layout_fails > 0) {
  for (const std::string& issue : layout_issues) {
    std::fprintf(stderr, "layout smoke: %s\n", issue.c_str());
  }
  self_test_mark("layout-fail");
  self_test_detach_maps(browser);
  return 30;
}
ui::views::MapViewport* map_pane = browser.map_viewport();
if (!map_pane || map_pane->bounds().width <= 0 ||
    map_pane->bounds().height <= 0) {
  self_test_detach_maps(browser);
  return 31;
}
self_test_mark("map-bounds-ok");
// Child HWND must track View bounds in the top-level client space
// (realize_native parents to Widget HWND; sync_native_bounds uses abs).
if (HWND map_hwnd = map_pane->native_view()) {
  if (!IsWindow(map_hwnd)) {
    self_test_detach_maps(browser);
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
    std::fprintf(stderr,
                 "map hwnd origin (%ld,%ld) vs view (%d,%d)\n", tl.x, tl.y,
                 vb.x, vb.y);
    self_test_detach_maps(browser);
    return 33;
  }
  const int hw = wr.right - wr.left;
  const int hh = wr.bottom - wr.top;
  if (hw < vb.width - tol || hw > vb.width + tol ||
      hh < vb.height - tol || hh > vb.height + tol) {
    std::fprintf(stderr, "map hwnd size %dx%d vs view %dx%d\n", hw, hh,
                 vb.width, vb.height);
    self_test_detach_maps(browser);
    return 34;
  }
} else {
  self_test_detach_maps(browser);
  return 35;
}
self_test_mark("hwnd-sync-ok");
if (ui::views::View* root_view = browser.contents_view()) {
  if (ui::views::View* menu = root_view->child_at(0)) {
    if (menu->bounds().height <
        ui::views::dip_to_px(22, 1.f)) {
      self_test_detach_maps(browser);
      return 32;
    }
  }
}
  return 0;
}

}  // namespace detail
}  // namespace app
