// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/input/input_showcase.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/public/map_types.h"
#include "content/public/view_host.h"
#include "gis/edit/memory_session.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {
namespace {

void mark(const char* step) {
  detail::write_mark(detail::kInputShowcaseMarkLeaf, step, /*truncate=*/true);
}

content::InputEvent make_ldown(int x, int y) {
  content::InputEvent e{};
  e.kind = content::InputEvent::Kind::kLDown;
  e.x_px = x;
  e.y_px = y;
  return e;
}

content::InputEvent make_rdown(int x, int y) {
  content::InputEvent e{};
  e.kind = content::InputEvent::Kind::kRDown;
  e.x_px = x;
  e.y_px = y;
  return e;
}

bool expect_last_geom(gis::MemoryEditSession* mem, gis::FeatureGeom::Kind kind,
                      size_t min_points) {
  if (!mem || mem->committed_count() < 1) {
    return false;
  }
  const gis::FeatureMutation got =
      mem->committed_at(mem->committed_count() - 1);
  return got.op == gis::EditOp::kAppend && !got.geom.empty() &&
         got.geom.kind == kind && got.geom.points.size() >= min_points;
}

}  // namespace

int run_input_showcase(Browser& browser) {
  // Prefer suite script when present (full migration path); else C++ body.
  if (try_run_suite_script(browser, "input", detail::kInputShowcaseMarkLeaf)) {
    detail::detach_maps(browser);
    return 0;
  }

  // Always stop present timers + detach before return (ExitProcess heap race).
  struct DetachOnExit {
    Browser& browser;
    ~DetachOnExit() { detail::detach_maps(browser); }
  } detach_guard{browser};

  wchar_t mark_path[MAX_PATH] = {};
  if (detail::exe_capture_path(mark_path, MAX_PATH,
                               L"input-self-test-mark.txt")) {
    DeleteFileW(mark_path);
  }
  mark("show");
  detail::pump_messages(300);
  if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
    return 2;
  }
  mark("hwnd-ok");

  browser.select_map_tab(0);
  detail::pump_messages(400);
  // Digitize / FeatureGeom only needs ViewHost + Workspace ?do not hard-fail
  // on ContentMapView first-frame latency (full --self-test covers that).
  if (ui::views::DrawHost* map = browser.draw_host()) {
    (void)map->wait_ready(5000);
  }
  mark("map-ready");

  content::ViewHost* host = browser.edit_view_host();
  if (!host || !host->workspace() || !host->edits()) {
    return 11;
  }
  auto* mem = dynamic_cast<gis::MemoryEditSession*>(host->edits());
  if (!mem) {
    return 11;
  }
  mark("host-ok");

  // Point
  if (!browser.run_tool_command("edit.append.point")) {
    return 12;
  }
  tool::Interaction* cur = host->workspace()->stack().current();
  if (!cur || std::strcmp(cur->id(), "draw.point") != 0) {
    return 13;
  }
  if (!host->dispatch_input(make_ldown(12, 18))) {
    return 14;
  }
  if (!expect_last_geom(mem, gis::FeatureGeom::Kind::kPoint, 1)) {
    return 15;
  }
  mark("input-point-ok");

  // Linestring: two verts + RMB finish
  if (!browser.run_tool_command("edit.append.linestring")) {
    return 60;
  }
  cur = host->workspace()->stack().current();
  if (!cur || std::strcmp(cur->id(), "draw.linestring") != 0) {
    return 60;
  }
  if (!host->dispatch_input(make_ldown(20, 20)) ||
      !host->dispatch_input(make_ldown(80, 60)) ||
      !host->dispatch_input(make_rdown(80, 60))) {
    return 60;
  }
  if (!expect_last_geom(mem, gis::FeatureGeom::Kind::kLineString, 2)) {
    return 60;
  }
  mark("input-line-ok");

  // Polygon: three verts + RMB finish
  if (!browser.run_tool_command("edit.append.polygon")) {
    return 64;
  }
  cur = host->workspace()->stack().current();
  if (!cur || std::strcmp(cur->id(), "draw.polygon") != 0) {
    return 64;
  }
  if (!host->dispatch_input(make_ldown(30, 30)) ||
      !host->dispatch_input(make_ldown(90, 30)) ||
      !host->dispatch_input(make_ldown(60, 80)) ||
      !host->dispatch_input(make_rdown(60, 80))) {
    return 64;
  }
  if (!expect_last_geom(mem, gis::FeatureGeom::Kind::kPolygon, 3)) {
    return 64;
  }
  mark("input-poly-ok");
  mark("input-ok");
  mark("pass");
  return 0;
}

}  // namespace app
