// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/product/traffic.h"

#include <windows.h>

#include <cstdio>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/showcase/plugin/capture/map2d_export.h"
#include "app/views/shell/harness/showcase/plugin/common/plugin_io.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/map_types.h"

namespace app {
namespace detail {

int run_traffic(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: traffic Map2d path\n");
  write_mark(kPluginShowcaseMarkLeaf, "traffic", /*truncate=*/true);

  browser.select_map_tab(0);
  pump_messages(200);

  char net_path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {L"..\\data\\plugin\\traffic_network_sample.geojson",
                           L"data\\plugin\\traffic_network_sample.geojson"};
  if (!resolve_rel_under_exe(rels, 2, net_path, sizeof(net_path))) {
    plugin_showcase_mark("traffic-sample-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("sample-ok");

  wchar_t out_w[MAX_PATH] = {};
  if (!exe_capture_path(out_w, MAX_PATH, L"plugin-showcase-traffic-path.geojson")) {
    plugin_showcase_mark("traffic-out-fail");
    detach_maps(browser);
    return 1;
  }
  char out_path[MAX_PATH * 3] = {};
  if (WideCharToMultiByte(CP_UTF8, 0, out_w, -1, out_path,
                          static_cast<int>(sizeof(out_path)), nullptr,
                          nullptr) <= 0) {
    plugin_showcase_mark("traffic-out-fail");
    detach_maps(browser);
    return 1;
  }

  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    plugin_showcase_mark("plugins-fail");
    detach_maps(browser);
    return 1;
  }

  const std::string net_esc = json_escape_path(net_path);
  const std::string out_esc = json_escape_path(out_path);
  const std::string args =
      std::string("{\"network\":\"") + net_esc + "\",\"output\":\"" + out_esc +
      "\",\"start_x\":116.335,\"start_y\":39.870,\"end_x\":116.452,"
      "\"end_y\":39.9285,\"weight_field\":\"cost\",\"frames\":12}";
  if (!browser.plugins()->run_processing("traffic.cost_path", args)) {
    plugin_showcase_mark("traffic-run-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("traffic-ok");

  browser.fit_map_extent();
  pump_messages(400);
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  pump_messages(200);

  constexpr content::Extent2 kBeijing{116.2, 39.75, 116.55, 40.05};
  const bool bmp_ok =
      try_export_map2d_bmp(browser, "plugin-showcase-traffic.bmp", &kBeijing);
  if (bmp_ok) {
    plugin_showcase_mark("bmp-ok");
  } else {
    plugin_showcase_mark("bmp-skip");
  }
  plugin_showcase_mark("playback-ok");

  if (!bmp_ok) {
    detach_maps(browser);
    return 54;
  }
  plugin_showcase_mark("pass");
  detach_maps(browser);
  std::fprintf(stderr, "plugin-showcase: PASS mode=traffic (Map2d)\n");
  return 0;
}

}  // namespace detail
}  // namespace app
