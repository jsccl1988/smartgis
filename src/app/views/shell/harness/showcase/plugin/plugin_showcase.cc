// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/plugin_showcase.h"

#include <cstdio>
#include <cstdlib>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/showcase/plugin/product/mine.h"
#include "app/views/shell/harness/showcase/plugin/product/orthogrid.h"
#include "app/views/shell/harness/showcase/plugin/product/orthogrid3d.h"
#include "app/views/shell/harness/showcase/plugin/product/print.h"
#include "app/views/shell/harness/showcase/plugin/product/stormsurge.h"
#include "app/views/shell/harness/showcase/plugin/product/world3d.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "base/process/switches.h"

namespace app {

int plugin_showcase_body(Browser& browser, PluginShowcaseMode mode) {
  if (mode == PluginShowcaseMode::kWorld3d) {
    return detail::run_world3d_scene3d(browser);
  }
  if (mode == PluginShowcaseMode::kMine) {
    return detail::run_mine_scene3d(browser);
  }
  if (mode == PluginShowcaseMode::kStormSurge) {
    return detail::run_stormsurge_scene3d(browser);
  }
  if (mode == PluginShowcaseMode::kOrthogrid) {
    return detail::run_orthogrid(browser);
  }
  if (mode == PluginShowcaseMode::kOrthogrid3d) {
    return detail::run_orthogrid3d(browser);
  }
  if (mode == PluginShowcaseMode::kPrint) {
    return detail::run_print(browser);
  }
  // Other modes: Wave 2 suite bodies live in testing/tools/harness/plugin/*/*.il.
  (void)browser;
  detail::write_mark(detail::kPluginShowcaseMarkLeaf, "legacy-body-removed",
                     false);
  return 1;
}

int run_plugin_showcase(Browser& browser, PluginShowcaseMode mode) {
  // world3d / mine / stormsurge / orthogrid* / print use C++ bodies.
  if (mode == PluginShowcaseMode::kWorld3d ||
      mode == PluginShowcaseMode::kMine ||
      mode == PluginShowcaseMode::kStormSurge ||
      mode == PluginShowcaseMode::kOrthogrid ||
      mode == PluginShowcaseMode::kOrthogrid3d ||
      mode == PluginShowcaseMode::kPrint) {
    base::set_switch("force-gdi-map-overlay", "1");
    return plugin_showcase_body(browser, mode);
  }

  const char* suite = nullptr;
  switch (mode) {
    case PluginShowcaseMode::kPrint:
      suite = "plugin.print";
      break;
    case PluginShowcaseMode::kOrthogrid:
    case PluginShowcaseMode::kOrthogrid3d:
      break;
    case PluginShowcaseMode::kTraffic:
      suite = "plugin.traffic";
      break;
    case PluginShowcaseMode::kFlood:
      suite = "plugin.flood";
      break;
    case PluginShowcaseMode::kStormSurge:
      suite = "plugin.stormsurge";
      break;
    case PluginShowcaseMode::kMine:
      suite = "plugin.mine";
      break;
    case PluginShowcaseMode::kGeochem:
      suite = "plugin.geochem";
      break;
    case PluginShowcaseMode::kWorld3d:
    case PluginShowcaseMode::kNone:
      break;
  }
  base::set_switch("force-gdi-map-overlay", "1");
  if (suite &&
      try_run_suite_script(browser, suite, detail::kPluginShowcaseMarkLeaf,
                           /*clear_marks=*/true)) {
    std::fprintf(stderr, "plugin-showcase: PASS mode=%s (script)\n",
                 plugin_showcase_name(mode));
    detail::detach_maps(browser);
    return 0;
  }
  std::fprintf(stderr,
               "plugin-showcase: script failed mode=%s (resolve or exec)\n",
               suite ? suite : "(none)");
  detail::write_mark(detail::kPluginShowcaseMarkLeaf, "script-fail", false);
  detail::detach_maps(browser);
  return 1;
}

}  // namespace app
