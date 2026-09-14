// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Views host PE. Browser chrome is app::BrowserView; this file only
// dispatches ContentMain (--type=gpu / --type=renderer) and --self-test.

#include <windows.h>
#include <shellapi.h>

#include "app/views/browser_view.h"
#include "content/app/content_main.h"
#include "content/app/renderer_main.h"
#include "content/public/events.h"
#include "content/public/map_types.h"
#include "content/public/view_host.h"
#include "gpu/gpu.h"
#include "tool/interaction.h"
#include "tool/workspace.h"
#include "ui/views/catalog_view.h"
#include "ui/views/dpi.h"
#include "ui/views/map_viewport.h"
#include "ui/views/status_bar.h"
#include "ui/views/view.h"

#include <cstring>
#include <string>

namespace {

bool cmd_has_self_test() {
  const wchar_t* cmd = GetCommandLineW();
  return cmd && wcsstr(cmd, L"--self-test");
}

void pump_briefly(DWORD ms) {
  const DWORD end = GetTickCount() + ms;
  MSG msg;
  while (GetTickCount() < end) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
}

int BrowserMain(const content::ContentMainParams&) {
  ui::views::enable_process_dpi_awareness();
  const bool self_test = cmd_has_self_test();
  app::BrowserView browser;
  if (!browser.init()) {
    return 1;
  }
  browser.show();
  if (self_test) {
    pump_briefly(400);
    if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
      return 2;
    }
    // 2D Map Edit: require a presented frame when content map hang is active.
    ui::views::MapViewport* map = browser.map_viewport();
    if (map &&
        map->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView &&
        !map->wait_ready(20000)) {
      return 3;
    }
    ui::views::View* root = browser.contents_view();
    if (!root || root->child_count() < 3) {
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
    // 2D Data tab: same session; HWND must stay live after switch.
    browser.select_map_tab(1);
    pump_briefly(200);
    ui::views::MapViewport* data = browser.map_data_viewport();
    if (!data || !data->native_view() || !IsWindow(data->native_view())) {
      return 7;
    }
    if (data->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView &&
        !data->wait_ready(20000)) {
      return 8;
    }
    // 3D Scene tab: activate view3d.trackball and require a frame when hung.
    browser.select_map_tab(2);
    pump_briefly(400);
    ui::views::MapViewport* scene = browser.map_scene_viewport();
    if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
      return 9;
    }
    if (scene->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView &&
        !scene->wait_ready(20000)) {
      return 10;
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
    browser.select_map_tab(0);
    pump_briefly(100);
    content::ViewHost* host = browser.edit_view_host();
    if (!host || !host->workspace() || !host->edits()) {
      return 11;
    }
    if (!browser.run_tool_command("edit.append.point")) {
      return 12;
    }
    tool::Interaction* cur = host->workspace()->stack().current();
    if (!cur || std::strcmp(cur->id(), "draw.point") != 0) {
      return 13;
    }
    content::InputEvent down{};
    down.kind = content::InputEvent::Kind::kLDown;
    down.x_px = 12;
    down.y_px = 18;
    if (!host->dispatch_input(down)) {
      return 14;
    }
    if (!host->edits()->can_undo()) {
      return 15;
    }
    ui::views::StatusBar* status = browser.status_bar();
    if (!status || status->status().find("Committed") == std::string::npos) {
      return 16;
    }
    if (!browser.run_tool_command("selection.point")) {
      return 17;
    }
    cur = host->workspace()->stack().current();
    if (!cur || std::strcmp(cur->id(), "select.point") != 0) {
      return 18;
    }
    if (!browser.run_tool_command("selection.clear")) {
      return 19;
    }
    if (!status || status->status().find("Selection cleared") ==
                       std::string::npos) {
      return 20;
    }
    return 0;
  }
  return browser.run_loop();
}

int GpuMain(const content::ContentMainParams& params) {
  return gpu::GpuMain(params.argc, params.argv);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  content::ContentMainParams params;
  params.instance = instance;
  params.argc = argc;
  params.argv = argv;
  params.browser_main = &BrowserMain;
  params.gpu_main = &GpuMain;
  params.renderer_main = &content::RendererMain;
  const int rc = content::ContentMain(params);
  if (argv) {
    LocalFree(argv);
  }
  return rc;
}
