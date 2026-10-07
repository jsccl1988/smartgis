// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/traffic/scenario/run.h"

#include <windows.h>

#include <cstdio>
#include <string>

#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/types.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scenario/capture/map2d_export.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/shell.h"

namespace plugin {
namespace detail {

int run_traffic(HarnessShell& browser) {
  std::fprintf(stderr, "plugin-showcase: traffic Map2d path\n");
  browser.mark_named(plugin::kMarkPlugin, "traffic", /*truncate=*/true);
  plugin_mark("hwnd-ok");

  browser.select_view_tab(0);
  browser.pump(200);

  char net_path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {L"..\\data\\plugin\\traffic_network_sample.geojson",
                           L"data\\plugin\\traffic_network_sample.geojson"};
  if (!resolve_rel_under_exe(rels, 2, net_path, sizeof(net_path))) {
    plugin_mark("traffic-sample-fail");
    browser.detach_views();
    return 1;
  }
  plugin_mark("sample-ok");

  wchar_t out_w[MAX_PATH] = {};
  if (!browser.capture_path(out_w, MAX_PATH,
                            L"plugin-showcase-traffic-path.geojson")) {
    plugin_mark("traffic-out-fail");
    browser.detach_views();
    return 1;
  }
  char out_path[MAX_PATH * 3] = {};
  if (WideCharToMultiByte(CP_UTF8, 0, out_w, -1, out_path,
                          static_cast<int>(sizeof(out_path)), nullptr,
                          nullptr) <= 0) {
    plugin_mark("traffic-out-fail");
    browser.detach_views();
    return 1;
  }

  if (!browser.plugin_host()) {
    plugin_mark("plugins-fail");
    browser.detach_views();
    return 1;
  }

  const std::string net_esc = json_escape_path(net_path);
  const std::string out_esc = json_escape_path(out_path);
  // SW→NE on the Beijing lattice; cost_path picks the China RHT rightmost
  // equal-cost route (south arterial then east arterial).
  const std::string args =
      std::string("{\"network\":\"") + net_esc + "\",\"output\":\"" + out_esc +
      "\",\"start_x\":116.335,\"start_y\":39.870,\"end_x\":116.452,"
      "\"end_y\":39.9285,\"weight_field\":\"cost\",\"frames\":12}";
  // Drain present so GisDocument layers exist before Map2d BMP export.
  if (!run_processing_flushed(browser.plugin_host(), "traffic.cost_path",
                              args)) {
    plugin_mark("traffic-run-fail");
    browser.detach_views();
    return 1;
  }
  plugin_mark("traffic-ok");

  browser.fit_map_extent();
  browser.pump(400);
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  browser.pump(200);
  plugin_mark("extent-ok");

  // Tight framing around the Beijing lattice (not the wide city bbox).
  constexpr content::Extent2 kTrafficLattice{116.32, 39.86, 116.465, 39.94};
  const bool bmp_ok = try_export_map2d_bmp(
      browser, "plugin-showcase-traffic.bmp", &kTrafficLattice);
  if (bmp_ok) {
    plugin_mark("bmp-ok");
  } else {
    plugin_mark("bmp-skip");
  }
  plugin_mark("playback-ok");

  if (!bmp_ok) {
    browser.detach_views();
    return 54;
  }
  plugin_mark("pass");
  browser.detach_views();
  std::fprintf(stderr, "plugin-showcase: PASS mode=traffic (Map2d)\n");
  return 0;
}

}  // namespace detail
}  // namespace plugin
