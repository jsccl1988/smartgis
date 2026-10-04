// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/product/print.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/io/sample.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "gis/style/document/style_document.h"
#include "plugin/product/print/composer/print_composer.h"
#include "ui/views/map/map_viewport.h"
#include "vista/map/frame.h"

namespace app {
namespace detail {
namespace {

bool load_bmp_bgra(const char* path, std::vector<uint8_t>* bgra, int* w,
                   int* h, int* stride) {
  if (!path || !bgra || !w || !h || !stride) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  BITMAPFILEHEADER fh = {};
  BITMAPINFOHEADER ih = {};
  in.read(reinterpret_cast<char*>(&fh), sizeof(fh));
  in.read(reinterpret_cast<char*>(&ih), sizeof(ih));
  if (!in || fh.bfType != 0x4D42 || ih.biWidth <= 0 || ih.biHeight == 0) {
    return false;
  }
  const int width = ih.biWidth;
  const int height = std::abs(ih.biHeight);
  const int bpp = ih.biBitCount;
  if (bpp != 24 && bpp != 32) {
    return false;
  }
  const int row_bytes = ((width * bpp + 31) / 32) * 4;
  std::vector<uint8_t> raw(static_cast<size_t>(row_bytes) *
                           static_cast<size_t>(height));
  in.seekg(fh.bfOffBits, std::ios::beg);
  in.read(reinterpret_cast<char*>(raw.data()),
          static_cast<std::streamsize>(raw.size()));
  if (!in) {
    return false;
  }
  bgra->assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
               0);
  for (int y = 0; y < height; ++y) {
    const int src_y = (ih.biHeight > 0) ? y : (height - 1 - y);
    const uint8_t* src = raw.data() + static_cast<size_t>(src_y) * row_bytes;
    uint8_t* dst =
        bgra->data() + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
    for (int x = 0; x < width; ++x) {
      if (bpp == 32) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = 0xff;
        src += 4;
      } else {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = 0xff;
        src += 3;
      }
      dst += 4;
    }
  }
  *w = width;
  *h = height;
  *stride = width * 4;
  return true;
}

}  // namespace

int run_print(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: print layout path\n");
  write_mark(kPluginShowcaseMarkLeaf, "print", /*truncate=*/true);

  browser.select_map_tab(0);
  pump_messages(200);

  if (!try_open_china_sample(browser, /*write_stub_if_missing=*/true)) {
    plugin_showcase_mark("china-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("china-ok");

  apply_china_map2d_product_defaults(browser, 640, 480);
  browser.fit_map_extent();
  pump_messages(200);
  // Style + data: drop legacy text layers, blank name/anno/text on every
  // feature, and install print carto (no symbol layers). Default MapLibre
  // label slots twin-drew city glyphs under hillshade even after point-only
  // blanking when fit_map_extent rebuilt with default carto.
  if (content::MapScene* doc = browser.document()) {
    std::vector<std::string> drop_ids;
    for (const auto& layer : doc->layers()) {
      if (layer.name == "text" || layer.name == "anno" ||
          layer.name == "label" || layer.name == "注记") {
        drop_ids.push_back(layer.id);
      }
    }
    for (const std::string& id : drop_ids) {
      doc->remove_layer(id);
    }
    for (const auto& layer : doc->layers()) {
      for (const auto& f : layer.features) {
        const std::string tok = content::MapScene::feature_token(f.id);
        (void)doc->update_feature_field(tok, "name", "");
        (void)doc->update_feature_field(tok, "anno", "");
        (void)doc->update_feature_field(tok, "text", "");
      }
    }
    auto print_style = std::make_shared<gis::style::StyleDocument>();
    const std::string json = vista::print_carto_style_json();
    if (gis::style::parse_style_document(json, print_style.get())) {
      doc->set_style_document(std::move(print_style));
    }
  }
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    pane->invalidate_native();
  }
  pump_messages(200);

  wchar_t map_w[MAX_PATH] = {};
  wchar_t out_w[MAX_PATH] = {};
  if (!exe_capture_path(map_w, MAX_PATH, L"plugin-showcase-print-map.tmp.bmp") ||
      !exe_capture_path(out_w, MAX_PATH, L"plugin-showcase-print.bmp")) {
    plugin_showcase_mark("bmp-path-fail");
    detach_maps(browser);
    return 1;
  }
  char map_a[MAX_PATH] = {};
  char out_a[MAX_PATH] = {};
  if (WideCharToMultiByte(CP_ACP, 0, map_w, -1, map_a, MAX_PATH, nullptr,
                          nullptr) <= 0 ||
      WideCharToMultiByte(CP_ACP, 0, out_w, -1, out_a, MAX_PATH, nullptr,
                          nullptr) <= 0) {
    plugin_showcase_mark("bmp-path-fail");
    detach_maps(browser);
    return 1;
  }

  content::Map2dPresenter* map2d = browser.map2d();
  if (!map2d || !map2d->export_bmp(map_a, 640, 480)) {
    plugin_showcase_mark("map-export-fail");
    detach_maps(browser);
    return 1;
  }

  std::vector<uint8_t> map_bgra;
  int mw = 0;
  int mh = 0;
  int mstride = 0;
  if (!load_bmp_bgra(map_a, &map_bgra, &mw, &mh, &mstride)) {
    plugin_showcase_mark("map-read-fail");
    detach_maps(browser);
    return 1;
  }

  plugin::PrintComposerInput pin;
  pin.page_width_px = 640;
  pin.page_height_px = 480;
  pin.map_bgra = map_bgra.data();
  pin.map_width_px = mw;
  pin.map_height_px = mh;
  pin.map_stride_bytes = mstride;
  pin.map_units_per_px = 12000.0;
  pin.scale_label = "1:12000000";
  pin.legend = {{"Land", 0xff88aa66},
                {"Roads", 0xffccaa44},
                {"Rivers", 0xff1565c0},
                {"Cities", 0xffe65100}};

  if (!plugin::PrintComposer::export_page_bmp(pin, out_a)) {
    plugin_showcase_mark("compose-fail");
    detach_maps(browser);
    return 1;
  }
  DeleteFileA(map_a);
  plugin_showcase_mark("print-dialog-ok");
  plugin_showcase_mark("bmp-ok");
  plugin_showcase_mark("pass");
  detach_maps(browser);
  std::fprintf(stderr, "plugin-showcase: PASS mode=print (layout)\n");
  return 0;
}

}  // namespace detail
}  // namespace app
