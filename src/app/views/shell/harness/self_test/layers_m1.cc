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
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/shell/dpi.h"
#include "base/trace/event/process_trace.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/view/view.h"
#include "plugin/product/print/composer/print_composer.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>
#include <cwctype>

namespace app {
namespace detail {

int self_test_layers_m1(Browser& browser) {
// Document layers + features (Catalog / overlay paint). m0-save clears the
// seeded china pack and keeps a single round-trip linestring; polygon
// append adds a second feature ?require >= 2, not the pre-clear count.
if (!browser.document() || browser.document()->layer_count() == 0) {
  self_test_detach_maps(browser);
  return 36;
}
if (browser.document()->feature_count() < 2) {
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
// Prefer shared out/data/china_city.gpkg; else geojson; else china_plp.
{
  wchar_t sample_w[MAX_PATH] = {};
  if (exe_dir_with_slash(sample_w, MAX_PATH)) {
    char sample_a[MAX_PATH] = {};
    bool opened = false;
    bool city_pack = false;
    const wchar_t* candidates[] = {L"..\\data\\china_city.gpkg",
                                   L"..\\data\\china_city.geojson",
                                   L"..\\data\\china_plp.geojson",
                                   L"data\\china_city.gpkg",
                                   L"data\\china_city.geojson",
                                   L"data\\china_plp.geojson",
                                   L"china_city.gpkg",
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
      // Stub under shared out/data/ (exe is out/Debug|Release).
      wchar_t data_dir[MAX_PATH] = {};
      if (wcscpy_s(data_dir, sample_w) == 0 &&
          wcscat_s(data_dir, L"..\\data") == 0) {
        CreateDirectoryW(data_dir, nullptr);
      }
      wcscat_s(sample_w, L"..\\data\\views_ogr_selftest.geojson");
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
      // china_city.gpkg is area/line/point — city names live on point only
      // (no parallel text layer; see testing/data/build_china_city.py).
      if (browser.document()->layer_count() < 3) {
        std::fprintf(stderr,
                     "china_city pack expected >=3 layers (area/line/"
                     "point), got %zu\n",
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
      auto to_views_kind = [](content::LayerKind k) {
        switch (k) {
          case content::LayerKind::kGroup:
            return ui::views::LayerKind::kGroup;
          case content::LayerKind::kVector:
            return ui::views::LayerKind::kVector;
          case content::LayerKind::kRaster:
            return ui::views::LayerKind::kRaster;
          case content::LayerKind::kUnknown:
          default:
            return ui::views::LayerKind::kUnknown;
        }
      };
      std::function<ui::views::LayerTree::LayerDesc(const content::LayerDesc&)>
          convert = [&](const content::LayerDesc& d) {
            ui::views::LayerTree::LayerDesc row;
            row.id = d.id;
            row.name = d.name;
            row.visible = d.visible;
            row.active = d.active;
            row.kind = to_views_kind(d.kind);
            row.expanded = d.expanded;
            row.children.reserve(d.children.size());
            for (const content::LayerDesc& child : d.children) {
              row.children.push_back(convert(child));
            }
            return row;
          };
      std::vector<ui::views::LayerTree::LayerDesc> layers;
      for (const auto& d : browser.document()->layer_descs()) {
        layers.push_back(convert(d));
      }
      return layers;
    }());
    self_test_mark("ogr-ok");
    self_test_mark(city_pack ? "china-city-ok" : "china-plp-ok");

    // M1: labels + Style JSON + mock basemap + export BMP.
    {
      if (city_pack) {
        // Labels live on the point layer (no parallel "text" stem).
        bool found_point_layer = false;
        std::function<void(const content::LayerDesc&)> walk =
            [&](const content::LayerDesc& d) {
              if (d.name == "point") {
                found_point_layer = true;
              }
              for (const content::LayerDesc& child : d.children) {
                walk(child);
              }
            };
        for (const auto& d : browser.document()->layer_descs()) {
          walk(d);
          if (found_point_layer) {
            break;
          }
        }
        if (!found_point_layer) {
          std::fprintf(stderr, "M1: china_city missing point layer\n");
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
            std::string(style_path) + "..\\data\\china_city.style.json";
        style_loaded = browser.document()->load_style_path(cand);
        if (!style_loaded) {
          cand = std::string(style_path) + "data\\china_city.style.json";
          style_loaded = browser.document()->load_style_path(cand);
        }
        if (!style_loaded) {
          cand = std::string(style_path) + "china_city.style.json";
          style_loaded = browser.document()->load_style_path(cand);
        }
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
      // Shipped china_city.style.json uses Baidu cream (#f5f3e9); the
      // inline fallback above still uses the M1 green (#c8e6c9).
      const uint32_t expected_fill =
          style_loaded ? 0xFFF5F3E9u : 0xFFC8E6C9u;
      gis::style::ResolvedPaint rp;
      if (!browser.document()->resolve_style_for_test("area", {}, 10.0,
                                                      &rp) ||
          rp.fill_color != expected_fill) {
        std::fprintf(stderr,
                     "M1: style resolve failed fill=0x%08X expected=0x%08X "
                     "from_file=%d\n",
                     rp.fill_color, expected_fill,
                     style_loaded ? 1 : 0);
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
      // P0-3: one-page PrintComposer (map panel + scale + legend).
      {
        char layout_tmp[MAX_PATH] = {};
        if (GetTempPathA(MAX_PATH, layout_tmp) != 0) {
          const std::string layout_bmp =
              std::string(layout_tmp) + "smartgis_m1_layout.bmp";
          DeleteFileA(layout_bmp.c_str());
          plugin::PrintComposerInput pin;
          pin.page_width_px = 640;
          pin.page_height_px = 480;
          pin.map_units_per_px = 100.0;
          pin.scale_label = "1:100000";
          pin.legend = {{"Roads", 0xffccaa44}, {"Land", 0xff88aa66}};
          if (plugin::PrintComposer::export_page_bmp(pin, layout_bmp)) {
            FILE* lf = nullptr;
            if (fopen_s(&lf, layout_bmp.c_str(), "rb") == 0 && lf) {
              char magic[2] = {};
              const size_t ln = std::fread(magic, 1, 2, lf);
              std::fclose(lf);
              DeleteFileA(layout_bmp.c_str());
              if (ln == 2 && magic[0] == 'B' && magic[1] == 'M') {
                self_test_mark("m1-layout-ok");
              }
            } else {
              DeleteFileA(layout_bmp.c_str());
            }
          }
        }
      }
      // Restore product framing so later pan/wheel self-tests see the
      // real map HWND extent (M1 used a 256脙聴256 offscreen frame).
      if (ui::views::MapViewport* pane = browser.map_viewport()) {
        if (pane->native_view()) {
          browser.refit_active_view();
        }
      }
    }
  }
}
  return 0;
}

}  // namespace detail
}  // namespace app
