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

namespace app {
namespace detail {

int self_test_shell_ready(Browser& browser) {
wchar_t mark_path[MAX_PATH] = {};
if (exe_capture_path(mark_path, MAX_PATH, L"self-test-mark.txt")) {
  DeleteFileW(mark_path);
}
self_test_mark("show");
pump_views_messages_impl(400);
if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
  return 2;
}
self_test_mark("hwnd-ok");
// 2D Map Edit: require a presented frame when content map hang is active.
ui::views::DrawHost* map = browser.draw_host();
// ContentMapView (forced for --self-test) paints full-client DIBs via the
// GPU process. Debug + large clients often need >20s for the first frame
// of a freshly opened view; 90s matches exe_smoke's Views budget.
constexpr uint32_t k_frame_ready_ms = 90000;
if (map &&
    map->attach_mode() ==
        ui::views::DrawHost::AttachMode::kContentMapView) {
  if (!map->wait_ready(k_frame_ready_ms)) {
    return 3;
  }
  if (!viewport_has_presented_frame(map)) {
    return 3;
  }
  self_test_mark("map-frame-ok");
}
self_test_mark("map-ready");
// View tree walk below does not pump — leave Map Edit present running; the
// scene tab switch path pauses/hides it. Avoid KillTimer-only here (queued
// WM_TIMER + present_mu_ re-enter aborted as exit 3 under exe_smoke).
self_test_mark("post-map");
ui::views::View* root = browser.contents_view();
if (!root) {
  return 4;
}
self_test_mark("root-ok");
if (root->child_count() < 3) {
  return 4;
}
self_test_mark("child-count-ok");
ui::views::View* columns = root->child_at(1);
if (!columns || columns->child_count() < 2) {
  return 5;
}
self_test_mark("columns-ok");
ui::views::CatalogView* catalog = browser.catalog_view();
if (!catalog || !catalog->layer_tree()) {
  return 6;
}
self_test_mark("catalog-ok");
// Data tab removed: shell is Map (0) + 3D (1) only. Keep the mark so older
// harness mark lists still see a data-* token after Map is ready.
self_test_mark("data-ready");
// 3D Scene tab: activate view3d.trackball and require a frame when hung.
browser.select_map_tab(1);
pump_views_messages_impl(400);
ui::views::DrawHost* scene = browser.scene_draw_host();
if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
  std::fprintf(stderr, "self-test: scene host missing after tab switch\n");
  return 9;
}
// Inactive Map Edit HWND must hide so it does not cover the 3D pane.
if (map && map->native_view() && IsWindow(map->native_view()) &&
    IsWindowVisible(map->native_view())) {
  std::fprintf(stderr, "inactive map HWND still visible after 3D tab\n");
  return 36;
}
if (!IsWindowVisible(scene->native_view())) {
  std::fprintf(stderr, "active Scene HWND not visible\n");
  return 37;
}
if (scene->attach_mode() ==
        ui::views::DrawHost::AttachMode::kContentMapView) {
  scene->sync_native_bounds();
  scene->invalidate_native();
  pump_views_messages_impl(400);
  if (!scene->wait_ready(k_frame_ready_ms)) {
    return 10;
  }
  if (!viewport_has_presented_frame(scene)) {
    return 10;
  }
  self_test_mark("scene-frame-ok");
}
self_test_mark("scene-ready");
// Safe to stop present timers once Map/Scene have each produced a
// ContentMapView frame (or non-content attach). Drain queued ticks too.
if (map) {
  map->pause_present();
}
if (scene) {
  scene->pause_present();
}
// Placeholder / FlyCube / content are all acceptable; prove pan/orbit
// input reaches ViewHost without crashing when no GPU is present.
content::ViewHost* scene_host = scene->view_host();
if (!scene_host || !scene_host->workspace()) {
  return 21;
}
if (!scene_host->activate("view3d.trackball")) {
  return 22;
}
tool::Interaction* scene_tool = scene_host->workspace()->stack().current();
if (!scene_tool ||
    std::strcmp(scene_tool->id(), "view3d.trackball") != 0) {
  return 23;
}
content::InputEvent orbit_down{};
orbit_down.kind = content::InputEvent::Kind::kLDown;
orbit_down.x_px = 24;
orbit_down.y_px = 30;
content::InputEvent orbit_move{};
orbit_move.kind = content::InputEvent::Kind::kMouseMove;
orbit_move.x_px = 48;
orbit_move.y_px = 52;
content::InputEvent orbit_up{};
orbit_up.kind = content::InputEvent::Kind::kLUp;
orbit_up.x_px = 48;
orbit_up.y_px = 52;
if (!scene_host->dispatch_input(orbit_down) ||
    !scene_host->dispatch_input(orbit_move) ||
    !scene_host->dispatch_input(orbit_up)) {
  return 24;
}
if (!browser.orbit_frame() ||
    std::fabs(browser.orbit_frame()->yaw() - app::kScene3dDefaultYaw) <
        0.001f) {
  // Trackball drag must move the shell 3D camera (not a static mesh).
  self_test_detach_maps(browser);
  return 25;
}
self_test_mark("orbit-ok");
browser.select_map_tab(0);
pump_views_messages_impl(100);
  return 0;
}

}  // namespace detail
}  // namespace app
