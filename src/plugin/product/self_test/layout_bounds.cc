// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/self_test/probe.h"

#include "plugin/product/self_test/shell.h"
#include <windows.h>
#include <shellapi.h>

#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/app/content_main.h"
#include "content/embed/embed_sample.h"
#include "content/public/event_bus.h"
#include "content/public/map_contents.h"
#include "content/public/map_layer_types.h"
#include "content/public/view_host.h"
#include "content/renderer/renderer_main.h"
#include "gis/edit/memory_session.h"
#include "gis/style/document/style_document.h"
#include "gis/tile/provider/tile_provider.h"
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
#include "base/process/switches.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/map/viewport/draw_host.h"
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

namespace plugin {

int self_test_layout_bounds(SelfTestShell& browser) {
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
browser.mark("layout-checked");
const bool force_dump = [] {
  const char* v = base::switch_cstr("ui-forensics");
  return v && v[0] && !(v[0] == '0' && v[1] == '\0');
}();
if (layout_fails > 0 || force_dump) {
  // Mode A: dump text forensics on failure (or UI_FORENSICS=1).
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
          << "  \"exe\": \"SmartGIS.exe\",\n"
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
  browser.mark("layout-fail");
  browser.detach_maps();
  return 30;
}
ui::views::DrawHost* map_pane = browser.draw_host();
if (!map_pane || map_pane->bounds().width <= 0 ||
    map_pane->bounds().height <= 0) {
  browser.detach_maps();
  return 31;
}
browser.mark("map-bounds-ok");
// Child HWND must track View bounds in the top-level client space
// (realize_native parents to Widget HWND; sync_native_bounds uses abs).
if (HWND map_hwnd = map_pane->native_view()) {
  if (!IsWindow(map_hwnd)) {
    browser.detach_maps();
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
    browser.detach_maps();
    return 33;
  }
  const int hw = wr.right - wr.left;
  const int hh = wr.bottom - wr.top;
  if (hw < vb.width - tol || hw > vb.width + tol ||
      hh < vb.height - tol || hh > vb.height + tol) {
    std::fprintf(stderr, "map hwnd size %dx%d vs view %dx%d\n", hw, hh,
                 vb.width, vb.height);
    browser.detach_maps();
    return 34;
  }
} else {
  browser.detach_maps();
  return 35;
}
browser.mark("hwnd-sync-ok");
if (ui::views::View* root_view = browser.contents_view()) {
  if (ui::views::View* menu = root_view->child_at(0)) {
    if (menu->bounds().height <
        ui::views::dip_to_px(22, 1.f)) {
      browser.detach_maps();
      return 32;
    }
  }
}
  return 0;
}

}  // namespace plugin
