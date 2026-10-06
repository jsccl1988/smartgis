// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/showcase/map2d/seed/mode_seed.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/harness/showcase/map2d/seed/orthogrid_mesh.h"
#include "app/views/harness/showcase/map2d/common/progress.h"
#include "app/views/harness/showcase/map2d/seed/sample.h"
#include "content/browser/document/map_scene.h"
#include "content/public/plugin_host.h"
#include "plugin/product/map2d/commands.h"
#include "tool/command/command.h"

#include <cstdio>
#include <string>

namespace app {
namespace detail {
namespace {

bool execute_map2d_seed(Browser& browser, const char* mode_name) {
  PluginShell* shell = browser.plugins();
  if (!shell || !mode_name || !mode_name[0]) {
    return false;
  }
  content::PluginHost* host = shell->host();
  if (!host) {
    return false;
  }
  // Do not PluginShell::execute → ensure_builtins(): register_traffic currently
  // AVs (call 0x0) while enabling the full product table. Register map2d only,
  // then PluginHost::execute on this UI thread (same as chrome command dispatch).
  tool::CommandCatalog* catalog = shell->commands();
  if (!catalog || !catalog->contains("map2d.seed")) {
    if (!plugin::register_map2d(host)) {
      return false;
    }
  }
  const std::string payload =
      std::string("{\"mode\":\"") + mode_name + "\"}";
  tool::CommandArgs args;
  args.payload = payload;
  return host->execute("map2d.seed", args);
}

}  // namespace

int seed_map2d_mode(Browser& browser, Map2dShowcaseMode mode) {
  if (mode != Map2dShowcaseMode::kChina && mode != Map2dShowcaseMode::kAlign &&
      mode != Map2dShowcaseMode::kOrthogrid) {
    std::fprintf(stderr, "map2d-showcase: unsupported mode\n");
    return 53;
  }

  if (mode == Map2dShowcaseMode::kOrthogrid) {
    if (execute_map2d_seed(browser, "orthogrid")) {
      map2d_showcase_mark("orthogrid-ok");
      map2d_showcase_mark("map2d-seed-plugin");
      return 0;
    }
    if (!load_map2d_orthogrid_mesh(browser)) {
      std::fprintf(stderr, "map2d-showcase: orthogrid mesh failed\n");
      return 55;
    }
    map2d_showcase_mark("orthogrid-ok");
    return 0;
  }

  if (mode == Map2dShowcaseMode::kAlign) {
    if (!try_open_china_sample(browser)) {
      if (browser.document()) {
        browser.document()->seed_default();
      }
    }
    if (!browser.document() || browser.document()->feature_count() < 3) {
      std::fprintf(stderr, "map2d-showcase: china sample open failed\n");
      return 55;
    }
    map2d_showcase_mark("china-ok");
    if (execute_map2d_seed(browser, "align")) {
      map2d_showcase_mark("map2d-seed-plugin");
      return 0;
    }
    if (!try_load_align_style(browser)) {
      std::fprintf(stderr, "map2d-showcase: style_align.json load failed\n");
      return 55;
    }
    return 0;
  }

  if (!try_open_china_sample(browser)) {
    if (browser.document()) {
      browser.document()->seed_default();
    }
  }
  if (!browser.document() || browser.document()->feature_count() < 3) {
    std::fprintf(stderr, "map2d-showcase: china sample open failed\n");
    return 55;
  }
  map2d_showcase_mark("china-ok");
  if (execute_map2d_seed(browser, "china")) {
    map2d_showcase_mark("map2d-seed-plugin");
  }
  ensure_china_maplibre_carto(browser);
  map2d_showcase_mark("style-carto-default");
  return 0;
}

}  // namespace detail
}  // namespace app
