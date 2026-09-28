// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/self_test/self_test.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include <windows.h>
#include <shellapi.h>

#include "app/views/camera/map_host_extent.h"
#include "content/app/content_main.h"
#include "content/embed/embed_sample.h"
#include "content/public/event_bus.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/view_host.h"
#include "content/renderer/renderer_main.h"
#include "gis/model/edit/session/memory_edit_session.h"
#include "gis/present/style/style_document.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "gpu/gpu.h"
#include "net/http/http.h"
#include "render/rhi/rhi.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/views/gis/catalog/catalog_view.h"
#include "ui/views/gis/inspect/feature_info.h"
#include "ui/views/gis/shell/status_bar.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/view/view.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <cwctype>


namespace app {
namespace {

void pump_views_messages_impl(DWORD ms) {
  const DWORD end = GetTickCount() + ms;
  MSG msg;
  while (GetTickCount() < end) {
    while (GetTickCount() < end &&
           PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
}

void self_test_detach_maps(Browser& browser) {
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

void self_test_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  // Write next to the exe (out/), independent of process cwd.
  if (!detail::exe_sidecar_path(path, MAX_PATH, L"self-test-mark.txt")) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
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

}  // namespace

int run_views_self_test(Browser& browser) {
    wchar_t mark_path[MAX_PATH] = {};
    if (detail::exe_sidecar_path(mark_path, MAX_PATH, L"self-test-mark.txt")) {
      DeleteFileW(mark_path);
    }
    self_test_mark("show");
    pump_views_messages_impl(400);
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
    // Stop present timers on every map HWND before walking shell. Do not
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
    pump_views_messages_impl(200);
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
    pump_views_messages_impl(400);
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

    // M0: append linestring Ã¢Â?FeatureInfo Ã¢Â?write_path roundtrip.
    {
      const size_t before = browser.document()->feature_count();
      if (!browser.run_tool_command("edit.append.linestring")) {
        self_test_detach_maps(browser);
        return 60;
      }
      tool::Interaction* line_tool = host->workspace()->stack().current();
      if (!line_tool ||
          std::strcmp(line_tool->id(), "draw.linestring") != 0) {
        self_test_detach_maps(browser);
        return 60;
      }
      content::InputEvent v0{};
      v0.kind = content::InputEvent::Kind::kLDown;
      v0.x_px = 20;
      v0.y_px = 20;
      content::InputEvent v1{};
      v1.kind = content::InputEvent::Kind::kLDown;
      v1.x_px = 80;
      v1.y_px = 60;
      content::InputEvent fin{};
      fin.kind = content::InputEvent::Kind::kRDown;
      fin.x_px = 80;
      fin.y_px = 60;
      if (!host->dispatch_input(v0) || !host->dispatch_input(v1) ||
          !host->dispatch_input(fin)) {
        self_test_detach_maps(browser);
        return 60;
      }
      if (browser.document()->feature_count() <= before) {
        self_test_detach_maps(browser);
        return 60;
      }
      self_test_mark("m0-line-ok");

      std::vector<std::string> cols;
      std::vector<std::vector<std::string>> rows;
      std::vector<std::string> tokens;
      browser.document()->fill_attribute_rows(&cols, &rows, &tokens);
      if (tokens.empty()) {
        self_test_detach_maps(browser);
        return 61;
      }
      const content::FeatureId pick =
          app::MapScene::feature_id_from_token(tokens.back());
      if (!browser.document()->select_feature(pick)) {
        self_test_detach_maps(browser);
        return 61;
      }
      browser.refresh_inspectors();
      ui::views::FeatureInfo* info = browser.feature_info();
      if (!info || info->feature_id().empty()) {
        self_test_detach_maps(browser);
        return 61;
      }
      self_test_mark("m0-featureinfo-ok");

      char tmp[MAX_PATH] = {};
      if (GetTempPathA(MAX_PATH, tmp) == 0) {
        self_test_detach_maps(browser);
        return 62;
      }
      std::string out = std::string(tmp) + "smartgis_m0_selftest.geojson";
      DeleteFileA(out.c_str());
      if (!browser.document()->write_path(out)) {
        self_test_detach_maps(browser);
        return 62;
      }
      app::MapScene probe;
      if (!probe.open_path(out) || probe.feature_count() < 1) {
        DeleteFileA(out.c_str());
        self_test_detach_maps(browser);
        return 63;
      }
      DeleteFileA(out.c_str());
      self_test_mark("m0-save-ok");
    }

    if (!browser.run_tool_command("view.backend.maplibre")) {
      return 43;
    }
    if (!status || status->status().find("MapLibre") == std::string::npos) {
      return 44;
    }
    if (!browser.run_tool_command("view.backend.rhi")) {
      return 45;
    }
    if (!status || status->status().find("RHI") == std::string::npos) {
      return 46;
    }
    self_test_mark("backend-ok");
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
    // Prefer out/china_city.gpkg (Ã©ÂÂ¥Ã¦Â¶ÂÃ¦ÂµÂÃ§ÂÂ?; else geojson; else china_plp.
    {
      wchar_t sample_w[MAX_PATH] = {};
      if (detail::exe_dir_with_slash(sample_w, MAX_PATH)) {
        char sample_a[MAX_PATH] = {};
        bool opened = false;
        bool city_pack = false;
        const wchar_t* candidates[] = {L"china_city.gpkg",
                                       L"china_city.geojson",
                                       L"china_plp.geojson"};
        for (const wchar_t* name : candidates) {
          wchar_t china_w[MAX_PATH] = {};
          if (wcscpy_s(china_w, sample_w) != 0 ||
              wcscat_s(china_w, name) != 0) {
            continue;
          }
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
                "{\"type\":\"Feature\",\"properties\":{\"name\":\"Beijing\","
                "\"kind\":\"point\"},"
                "\"geometry\":{\"type\":\"Point\",\"coordinates\":"
                "[116.3974,39.9093]}},"
                "{\"type\":\"Feature\",\"properties\":{\"name\":\"Jingjin\","
                "\"kind\":\"line\"},"
                "\"geometry\":{\"type\":\"LineString\",\"coordinates\":"
                "[[116.3974,39.9093],[116.7,39.7],[117.2,39.12]]}},"
                "{\"type\":\"Feature\",\"properties\":{\"name\":\"Huabei\","
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
        self_test_mark(city_pack ? "china-city-ok" : "china-plp-ok");

        // M1: labels + Style JSON + mock basemap + export BMP.
        {
          if (city_pack) {
            bool found_text_layer = false;
            for (const auto& d : browser.document()->layer_descs()) {
              if (d.name == "text") {
                found_text_layer = true;
                break;
              }
            }
            if (!found_text_layer) {
              std::fprintf(stderr, "M1: china_city missing text layer\n");
              self_test_detach_maps(browser);
              return 70;
            }
          }
          self_test_mark("m1-labels-ok");

          char style_path[MAX_PATH] = {};
          bool style_loaded = false;
          if (GetModuleFileNameA(nullptr, style_path, MAX_PATH) > 0) {
            for (int i = static_cast<int>(std::strlen(style_path)) - 1; i >= 0;
                 --i) {
              if (style_path[i] == '\\' || style_path[i] == '/') {
                style_path[i + 1] = '\0';
                break;
              }
            }
            std::string cand =
                std::string(style_path) + "china_city.style.json";
            style_loaded = browser.document()->load_style_path(cand);
          }
          if (!style_loaded) {
            const char* kInline =
                "{\"version\":8,\"name\":\"m1\",\"layers\":[{"
                "\"id\":\"area-fill\",\"type\":\"fill\","
                "\"source-layer\":\"area\","
                "\"paint\":{\"fill-color\":\"#c8e6c9\"}}]}";
            auto doc = std::make_shared<gis::style::StyleDocument>();
            if (!gis::style::parse_style_document(kInline, doc.get())) {
              self_test_detach_maps(browser);
              return 71;
            }
            browser.document()->set_style_document(std::move(doc));
          }
          gis::style::ResolvedPaint rp;
          if (!browser.document()->resolve_style_for_test("area", {}, 10.0,
                                                          &rp) ||
              rp.fill_color != 0xFFC8E6C9u) {
            std::fprintf(stderr, "M1: style resolve failed\n");
            self_test_detach_maps(browser);
            return 71;
          }
          self_test_mark("m1-style-ok");

          auto provider = std::make_shared<gis::tile::TileProvider>();
          if (!provider->open_xyz("http://tiles.local/{z}/{x}/{y}.png")) {
            self_test_detach_maps(browser);
            return 72;
          }
          provider->set_fetch_fn([](const std::string&) {
            net::HttpResult res;
            res.ok = true;
            res.status = 200;
            res.body = "PNG-STUB";
            return res;
          });
          browser.document()->set_basemap_provider(provider);
          browser.view_frame()->apply_world_extent(app::kChinaLonLatExtent, 256,
                                                  256);
          HDC screen = GetDC(nullptr);
          HDC mem = CreateCompatibleDC(screen);
          BITMAPINFO bmi = {};
          bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
          bmi.bmiHeader.biWidth = 256;
          bmi.bmiHeader.biHeight = -256;
          bmi.bmiHeader.biPlanes = 1;
          bmi.bmiHeader.biBitCount = 32;
          bmi.bmiHeader.biCompression = BI_RGB;
          void* bits = nullptr;
          HBITMAP dib =
              CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
          if (!dib) {
            DeleteDC(mem);
            ReleaseDC(nullptr, screen);
            self_test_detach_maps(browser);
            return 72;
          }
          HGDIOBJ old = SelectObject(mem, dib);
          browser.map2d()->paint(mem, 256, 256);
          const size_t tiles = browser.map2d()->basemap_tiles_drawn();
          SelectObject(mem, old);
          DeleteObject(dib);
          DeleteDC(mem);
          ReleaseDC(nullptr, screen);
          if (tiles == 0) {
            std::fprintf(stderr, "M1: basemap drew zero tiles\n");
            self_test_detach_maps(browser);
            return 72;
          }
          self_test_mark("m1-basemap-ok");

          char tmp[MAX_PATH] = {};
          if (GetTempPathA(MAX_PATH, tmp) == 0) {
            self_test_detach_maps(browser);
            return 73;
          }
          std::string bmp = std::string(tmp) + "smartgis_m1_selftest.bmp";
          DeleteFileA(bmp.c_str());
          if (!browser.map2d()->export_bmp(bmp, 320, 240)) {
            self_test_detach_maps(browser);
            return 73;
          }
          FILE* bf = nullptr;
          if (fopen_s(&bf, bmp.c_str(), "rb") != 0 || !bf) {
            self_test_detach_maps(browser);
            return 73;
          }
          char magic[2] = {};
          const size_t n = std::fread(magic, 1, 2, bf);
          std::fclose(bf);
          DeleteFileA(bmp.c_str());
          if (n != 2 || magic[0] != 'B' || magic[1] != 'M') {
            self_test_detach_maps(browser);
            return 73;
          }
          self_test_mark("m1-export-ok");
          // Restore product framing so later pan/wheel self-tests see the
          // real map HWND extent (M1 used a 256Ã256 offscreen frame).
          if (ui::views::MapViewport* pane = browser.map_viewport()) {
            if (pane->native_view()) {
              browser.refit_active_view();
            }
          }
        }
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
    // Wheel-to-cursor must change overlay scale (not view-center zoom).
    {
      content::ViewHost* host = browser.edit_view_host();
      const double scale0 = browser.view_frame()->scale();
      content::InputEvent wheel{};
      wheel.kind = content::InputEvent::Kind::kWheel;
      wheel.x_px = 40;
      wheel.y_px = 40;
      wheel.wheel = 120;
      if (!host || !host->dispatch_input(wheel) ||
          std::fabs(browser.view_frame()->scale() - scale0) < 1e-9) {
        self_test_detach_maps(browser);
        return 47;
      }
      const render::rhi::CameraMatrices ortho =
          browser.orbit_frame()->camera_matrices_ortho(800.f, 600.f);
      if (ortho.kind != render::rhi::CameraKind::kOrtho) {
        self_test_detach_maps(browser);
        return 48;
      }
      self_test_mark("wheel-cursor-ok");
    }
    // FlyCube orbit camera matrices must track shell yaw/pitch.
    {
      const float yaw_after = browser.orbit_frame()->yaw();
      const render::rhi::CameraMatrices cam =
          browser.orbit_frame()->camera_matrices(1.333f);
      if (cam.kind != render::rhi::CameraKind::kPerspective ||
          std::fabs(yaw_after - app::kScene3dDefaultYaw) < 0.001f) {
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
        // Optional atmosphere exercise: demo on for self-test only; normal
        // launches leave ocean/cloud disabled.
        browser.scene3d()->enable_atmosphere_demo();
        self_test_mark("atmosphere-demo");
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

    // M2: Processing panel + buffer/clip write-back.
    {
      std::string err;
      if (!browser.run_m2_self_test_hooks(&err)) {
        std::fprintf(stderr, "M2 self-test failed: %s\n", err.c_str());
        self_test_detach_maps(browser);
        if (err.find("clip") != std::string::npos) {
          return 82;
        }
        if (err.find("buffer") != std::string::npos ||
            err.find("write-back") != std::string::npos ||
            err.find("write_path") != std::string::npos) {
          return 81;
        }
        return 80;
      }
      self_test_mark("m2-panel-ok");
      self_test_mark("m2-buffer-ok");
      self_test_mark("m2-clip-ok");
    }

    // M3: DEM seed + tileset stream + atmosphere toggle.
    {
      std::string err;
      if (!browser.scene3d()->run_m3_self_test_hooks(&err)) {
        std::fprintf(stderr, "M3 self-test failed: %s\n", err.c_str());
        self_test_detach_maps(browser);
        if (err == "m3-dem-ok") {
          return 90;
        }
        if (err == "m3-tiles-ok") {
          return 91;
        }
        return 92;
      }
      self_test_mark("m3-dem-ok");
      self_test_mark("m3-tiles-ok");
      self_test_mark("m3-atmosphere-ok");
    }

    // M4: optimistic edit conflict + content:: embed open path.
    {
      content::FeatureId fid{};
      fid.len = 1;
      fid.bytes[0] = 42;
      auto store = std::make_shared<gis::OptimisticLayerStore>();
      store->seed_feature(fid, 1);
      gis::MemoryEditSession client_a(store);
      gis::MemoryEditSession client_b(store);
      gis::FeatureMutation write{};
      write.op = gis::EditOp::kModify;
      write.id = fid;
      write.base_version = 1;
      if (!client_a.commit(write) ||
          store->feature_version(fid) != 2 ||
          client_b.commit(write) ||
          client_b.last_status() != gis::CommitStatus::kConflict) {
        std::fprintf(stderr, "M4 conflict self-test failed\n");
        self_test_detach_maps(browser);
        return content::kExitEditConflict;
      }
      self_test_mark("m4-conflict-ok");

      content::EmbedMapHost embed;
      if (!content::open_map_host_path(&embed, "map://self-test") ||
          embed.path != "map://self-test") {
        std::fprintf(stderr, "M4 embed self-test failed\n");
        self_test_detach_maps(browser);
        return content::kExitEmbedOpenFailed;
      }
      self_test_mark("m4-embed-ok");
    }

    self_test_mark("pass");
    self_test_detach_maps(browser);
    self_test_mark("detached");
    return 0;
}

void pump_views_messages(DWORD ms) { pump_views_messages_impl(ms); }

}  // namespace app
