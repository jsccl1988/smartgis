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
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/map_widget_host_view.h"
#include "content/public/view_host.h"
#include "gpu/gpu.h"
#include "render/rhi/rhi.h"
#include "tool/interaction.h"
#include "tool/workspace.h"
#include "ui/views/catalog_view.h"
#include "ui/views/dpi.h"
#include "ui/views/layout_check.h"
#include "ui/views/map_viewport.h"
#include "ui/views/menu_bar.h"
#include "ui/views/status_bar.h"
#include "ui/views/view.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

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

void self_test_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return;
  }
  // Write next to the exe (out/), independent of process cwd.
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i + 1] = L'\0';
      break;
    }
  }
  if (wcscat_s(path, L"self-test-mark.txt") != 0) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
  }
}

void self_test_detach_maps(app::BrowserView& browser) {
  if (browser.scene3d()) {
    browser.scene3d()->abandon_mesh();
  }
  if (ui::views::MapViewport* m = browser.map_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_data_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_scene_viewport()) {
    m->detach();
  }
}

bool viewport_has_presented_frame(ui::views::MapViewport* pane) {
  if (!pane ||
      pane->attach_mode() !=
          ui::views::MapViewport::AttachMode::kContentMapView) {
    return false;
  }
  content::MapContents* session = pane->map_contents();
  if (!session || pane->view_id() == 0) {
    return false;
  }
  content::MapWidgetHostView* view = session->HostView(pane->view_id());
  if (!view) {
    return false;
  }
  const content::SharedSurface surface = view->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px >= 8 && surface.height_px >= 8;
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
    wchar_t mark_path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, mark_path, MAX_PATH) > 0) {
      for (int i = static_cast<int>(wcslen(mark_path)) - 1; i >= 0; --i) {
        if (mark_path[i] == L'\\' || mark_path[i] == L'/') {
          mark_path[i + 1] = L'\0';
          break;
        }
      }
      wcscat_s(mark_path, L"self-test-mark.txt");
      DeleteFileW(mark_path);
    }
    self_test_mark("show");
    pump_briefly(400);
    if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
      return 2;
    }
    self_test_mark("hwnd-ok");
    // 2D Map Edit: require a presented frame when content map hang is active.
    ui::views::MapViewport* map = browser.map_viewport();
    if (map &&
        map->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView) {
      if (!map->wait_ready(20000)) {
        return 3;
      }
      if (!viewport_has_presented_frame(map)) {
        return 3;
      }
      self_test_mark("map-frame-ok");
    }
    self_test_mark("map-ready");
    // Stop present timers on every map HWND before walking chrome. Do not
    // pump here: PeekMessage would deliver WM_PAINT and race Widget paint
    // buffers / MapViewport backbuffers (AV after map-ready).
    auto stop_present = [](ui::views::MapViewport* pane) {
      if (pane && pane->native_view() && IsWindow(pane->native_view())) {
        KillTimer(pane->native_view(), 1);  // kPresentTimerId == 1
      }
    };
    stop_present(map);
    stop_present(browser.map_data_viewport());
    stop_present(browser.map_scene_viewport());
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
    // 2D Data tab: same session; HWND must stay live after switch.
    browser.select_map_tab(1);
    pump_briefly(200);
    ui::views::MapViewport* data = browser.map_data_viewport();
    if (!data || !data->native_view() || !IsWindow(data->native_view())) {
      return 7;
    }
    // Inactive Map Edit HWND must hide so it does not cover the Data pane.
    if (map && map->native_view() && IsWindow(map->native_view()) &&
        IsWindowVisible(map->native_view())) {
      std::fprintf(stderr, "inactive map HWND still visible after Data tab\n");
      return 36;
    }
    if (!IsWindowVisible(data->native_view())) {
      std::fprintf(stderr, "active Data HWND not visible\n");
      return 37;
    }
    if (data->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView &&
        !data->wait_ready(20000)) {
      return 8;
    }
    self_test_mark("data-ready");
    // 3D Scene tab: activate view3d.trackball and require a frame when hung.
    browser.select_map_tab(2);
    pump_briefly(400);
    ui::views::MapViewport* scene = browser.map_scene_viewport();
    if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
      return 9;
    }
    if (scene->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView) {
      if (!scene->wait_ready(20000)) {
        return 10;
      }
      if (!viewport_has_presented_frame(scene)) {
        return 10;
      }
      self_test_mark("scene-frame-ok");
    }
    self_test_mark("scene-ready");
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
    if (!browser.scene3d() ||
        std::fabs(browser.scene3d()->yaw() - 0.55f) < 0.001f) {
      // Trackball drag must move the chrome 3D camera (not a static mesh).
      self_test_detach_maps(browser);
      return 25;
    }
    self_test_mark("orbit-ok");
    browser.select_map_tab(0);
    pump_briefly(100);
    content::ViewHost* host = browser.edit_view_host();
    if (!host || !host->workspace() || !host->edits()) {
      return 11;
    }
    if (!browser.run_tool_command("edit.append.point")) {
      return 12;
    }
    self_test_mark("edit-point");
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
    self_test_mark("selection-ok");
    // Document layers + features (Catalog / overlay paint).
    if (!browser.document() || browser.document()->layer_count() == 0) {
      self_test_detach_maps(browser);
      return 36;
    }
    if (browser.document()->feature_count() < 3) {
      self_test_detach_maps(browser);
      return 37;
    }
    if (ui::views::CatalogView* cat = browser.catalog_view()) {
      if (!cat->layer_tree() || cat->layer_tree()->layer_count() == 0) {
        self_test_detach_maps(browser);
        return 38;
      }
    }
    self_test_mark("layers-ok");
    // Prefer out/china_city.gpkg (四图层); else geojson; else china_plp.
    {
      wchar_t sample_w[MAX_PATH] = {};
      if (GetModuleFileNameW(nullptr, sample_w, MAX_PATH) > 0) {
        for (int i = static_cast<int>(wcslen(sample_w)) - 1; i >= 0; --i) {
          if (sample_w[i] == L'\\' || sample_w[i] == L'/') {
            sample_w[i + 1] = L'\0';
            break;
          }
        }
        char sample_a[MAX_PATH] = {};
        bool opened = false;
        bool city_pack = false;
        const wchar_t* candidates[] = {L"china_city.gpkg",
                                       L"china_city.geojson",
                                       L"china_plp.geojson"};
        for (const wchar_t* name : candidates) {
          wchar_t china_w[MAX_PATH] = {};
          wcscpy_s(china_w, sample_w);
          wcscat_s(china_w, name);
          if (GetFileAttributesW(china_w) == INVALID_FILE_ATTRIBUTES) {
            continue;
          }
          WideCharToMultiByte(CP_UTF8, 0, china_w, -1, sample_a, MAX_PATH,
                              nullptr, nullptr);
          if (browser.document()->open_path(sample_a) &&
              browser.document()->last_open_was_ogr()) {
            opened = true;
            city_pack = (wcsstr(name, L"china_city") != nullptr);
            break;
          }
        }
        if (!opened) {
          wcscat_s(sample_w, L"views_ogr_selftest.geojson");
          FILE* sf = nullptr;
          if (_wfopen_s(&sf, sample_w, L"wb") == 0 && sf) {
            static const char kGeojson[] =
                "{\"type\":\"FeatureCollection\",\"name\":\"china_plp\","
                "\"features\":["
                "{\"type\":\"Feature\",\"properties\":{\"name\":\"北京点\","
                "\"kind\":\"point\"},"
                "\"geometry\":{\"type\":\"Point\",\"coordinates\":"
                "[116.3974,39.9093]}},"
                "{\"type\":\"Feature\",\"properties\":{\"name\":\"京津走廊\","
                "\"kind\":\"line\"},"
                "\"geometry\":{\"type\":\"LineString\",\"coordinates\":"
                "[[116.3974,39.9093],[116.7,39.7],[117.2,39.12]]}},"
                "{\"type\":\"Feature\",\"properties\":{\"name\":\"华北面\","
                "\"kind\":\"area\"},"
                "\"geometry\":{\"type\":\"Polygon\",\"coordinates\":"
                "[[[116.2,39.7],[116.8,39.7],[116.8,40.1],[116.2,40.1],"
                "[116.2,39.7]]]}}"
                "]}";
            std::fwrite(kGeojson, 1, sizeof(kGeojson) - 1, sf);
            std::fclose(sf);
            WideCharToMultiByte(CP_UTF8, 0, sample_w, -1, sample_a, MAX_PATH,
                                nullptr, nullptr);
            opened = browser.document()->open_path(sample_a) &&
                     browser.document()->last_open_was_ogr();
          }
        }
        if (!opened || browser.document()->feature_count() < 3 ||
            browser.document()->layer_count() == 0) {
          std::fprintf(stderr, "OGR China map self-test open failed: %s\n",
                       sample_a);
          self_test_detach_maps(browser);
          return 26;
        }
        if (city_pack) {
          if (browser.document()->layer_count() < 4) {
            std::fprintf(stderr,
                         "china_city pack expected >=4 layers (area/line/"
                         "point/text), got %zu\n",
                         browser.document()->layer_count());
            self_test_detach_maps(browser);
            return 26;
          }
          if (browser.document()->feature_count() < 200) {
            std::fprintf(stderr,
                         "china_city pack expected >=200 features, got %zu\n",
                         browser.document()->feature_count());
            self_test_detach_maps(browser);
            return 26;
          }
        }
        if (!browser.document()->has_china_extent()) {
          std::fprintf(stderr, "OGR China extent not in China lon/lat\n");
          self_test_detach_maps(browser);
          return 39;
        }
        browser.catalog_view()->populate_layers([&] {
          std::vector<ui::views::LayerTree::LayerDesc> layers;
          for (const auto& d : browser.document()->layer_descs()) {
            ui::views::LayerTree::LayerDesc row;
            row.id = d.id;
            row.name = d.name;
            row.visible = d.visible;
            row.active = d.active;
            layers.push_back(std::move(row));
          }
          return layers;
        }());
        self_test_mark("ogr-ok");
        self_test_mark("china-plp-ok");
      }
    }
    // Pan tool must activate without crash (Map tab).
    if (!browser.run_tool_command("view.pan")) {
      self_test_detach_maps(browser);
      return 40;
    }
    {
      content::ViewHost* host = browser.edit_view_host();
      tool::Interaction* cur =
          host && host->workspace() ? host->workspace()->stack().current()
                                    : nullptr;
      if (!cur || std::strcmp(cur->id(), "view.pan") != 0) {
        self_test_detach_maps(browser);
        return 41;
      }
      content::InputEvent pan_down{};
      pan_down.kind = content::InputEvent::Kind::kLDown;
      pan_down.x_px = 40;
      pan_down.y_px = 40;
      content::InputEvent pan_move{};
      pan_move.kind = content::InputEvent::Kind::kMouseMove;
      pan_move.x_px = 70;
      pan_move.y_px = 55;
      content::InputEvent pan_up{};
      pan_up.kind = content::InputEvent::Kind::kLUp;
      pan_up.x_px = 70;
      pan_up.y_px = 55;
      if (!host->dispatch_input(pan_down) || !host->dispatch_input(pan_move) ||
          !host->dispatch_input(pan_up)) {
        self_test_detach_maps(browser);
        return 42;
      }
      self_test_mark("pan-ok");
    }
    // FlyCube orbit camera matrices must track chrome yaw/pitch.
    {
      const float yaw_after = browser.scene3d()->yaw();
      const render::rhi::CameraMatrices cam =
          browser.scene3d()->camera_matrices(1.333f);
      if (cam.kind != render::rhi::CameraKind::kPerspective ||
          std::fabs(yaw_after - 0.55f) < 0.001f) {
        self_test_detach_maps(browser);
        return 27;
      }
      // View matrix must not be identity after orbit.
      bool view_moved = false;
      for (int i = 0; i < 16; ++i) {
        const float ident = (i % 5 == 0) ? 1.f : 0.f;
        if (std::fabs(cam.view[i] - ident) > 1e-4f) {
          view_moved = true;
          break;
        }
      }
      if (!view_moved) {
        self_test_detach_maps(browser);
        return 28;
      }
      if (scene &&
          scene->attach_mode() ==
              ui::views::MapViewport::AttachMode::kFlyCube &&
          scene->rhi_device()) {
        if (!browser.scene3d()->present_gpu(
                static_cast<render::rhi::Device*>(scene->rhi_device()), 64,
                64)) {
          std::fprintf(stderr, "FlyCube present_gpu after orbit failed\n");
          self_test_detach_maps(browser);
          return 29;
        }
        self_test_mark("flycube-camera-ok");
      } else {
        self_test_mark("flycube-skipped");
      }
    }
    // Layout smoke: bounds non-negative, children inside parents.
    std::vector<std::string> layout_issues;
    const int layout_fails =
        ui::views::collect_layout_violations(browser.contents_view(),
                                             &layout_issues);
    self_test_mark("layout-checked");
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
    self_test_mark("pass");
    self_test_detach_maps(browser);
    self_test_mark("detached");
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
