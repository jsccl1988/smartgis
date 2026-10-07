// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/flood/scenario/run.h"

#include <windows.h>

#include <cstdio>
#include <string>

#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/map_layer_types.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scenario/capture/map2d_export.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/shell.h"
namespace plugin {
namespace detail {

int run_flood(HarnessShell& browser) {
  std::fprintf(stderr, "plugin-showcase: flood Map2d path\n");
  browser.mark_named(plugin::kMarkPlugin, "flood", /*truncate=*/true);
  plugin_mark("hwnd-ok");

  browser.select_map_tab(0);
  browser.pump(200);

  char dem_path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {L"..\\data\\plugin\\flood_basin_sample.tif",
                           L"data\\plugin\\flood_basin_sample.tif"};
  if (!resolve_rel_under_exe(rels, 2, dem_path, sizeof(dem_path))) {
    plugin_mark("flood-sample-fail");
    browser.detach_maps();
    return 1;
  }
  plugin_mark("sample-ok");

  // Output path need not exist yet (peer IL sidecar_path).
  wchar_t out_w[MAX_PATH] = {};
  if (!browser.capture_path(out_w, MAX_PATH, L"flood_mask.tif")) {
    plugin_mark("flood-out-fail");
    browser.detach_maps();
    return 1;
  }
  char out_path[MAX_PATH * 3] = {};
  if (WideCharToMultiByte(CP_UTF8, 0, out_w, -1, out_path,
                          static_cast<int>(sizeof(out_path)), nullptr,
                          nullptr) <= 0) {
    plugin_mark("flood-out-fail");
    browser.detach_maps();
    return 1;
  }
  plugin_mark("out-ok");

  if (!browser.plugin_host()) {
    plugin_mark("plugins-fail");
    browser.detach_maps();
    return 1;
  }

  const std::string dem_esc = json_escape_path(dem_path);
  const std::string out_esc = json_escape_path(out_path);
  const std::string args =
      std::string("{\"dem\":\"") + dem_esc + "\",\"output\":\"" + out_esc +
      "\",\"seed_x\":114.30,\"seed_y\":30.55,\"water_level\":45.0,"
      "\"frames\":8}";
  if (!run_processing_flushed(browser.plugin_host(), "flood.inundate", args)) {
    plugin_mark("flood-run-fail");
    browser.detach_maps();
    return 1;
  }
  plugin_mark("flood-ok");

  browser.fit_map_extent();
  browser.pump(400);
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  browser.pump(200);
  plugin_mark("extent-ok");

  constexpr content::Extent2 kWuhan{114.15, 30.45, 114.45, 30.65};
  const bool bmp_ok =
      try_export_map2d_bmp(browser, "plugin-showcase-flood.bmp", &kWuhan);
  if (bmp_ok) {
    plugin_mark("bmp-ok");
  } else {
    plugin_mark("bmp-skip");
  }
  plugin_mark("playback-ok");

  if (!bmp_ok) {
    browser.detach_maps();
    return 54;
  }
  plugin_mark("pass");
  browser.detach_maps();
  std::fprintf(stderr, "plugin-showcase: PASS mode=flood (Map2d)\n");
  return 0;
}

}  // namespace detail
}  // namespace plugin
