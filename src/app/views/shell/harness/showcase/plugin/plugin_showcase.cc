// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/plugin_showcase.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/views/shell/app/cmdline/views_launch_options.h"
#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/showcase/plugin/product/mine.h"
#include "app/views/shell/harness/showcase/plugin/product/orthogrid.h"
#include "app/views/shell/harness/showcase/plugin/product/orthogrid3d.h"
#include "app/views/shell/harness/showcase/plugin/product/print.h"
#include "app/views/shell/harness/showcase/plugin/product/report.h"
#include "app/views/shell/harness/showcase/plugin/product/stormsurge.h"
#include "app/views/shell/harness/showcase/plugin/product/traffic.h"
#include "app/views/shell/harness/showcase/plugin/product/world3d.h"
#include "app/views/shell/harness/showcase/plugin/product/world_preview.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "base/process/switches.h"

namespace app {
namespace {

std::string suite_id_for_mode(const std::string& mode) {
  if (mode.empty()) {
    return {};
  }
  return "plugin." + mode;
}

// Harness-only C++ bodies. CapabilityHost / PluginShell must not grow a peer
// product switch — new suites prefer testing/tools/harness/plugin/*/*.il.
struct ShowcaseBody {
  const char* mode;
  bool map2d_face;
  int (*run)(Browser&);
};

constexpr ShowcaseBody kCppBodies[] = {
    {"world3d", false, &detail::run_world3d_scene3d},
    {"world_preview", false, &detail::run_world_preview},
    {"mine", false, &detail::run_mine_scene3d},
    {"stormsurge", false, &detail::run_stormsurge_scene3d},
    {"orthogrid", true, &detail::run_orthogrid},
    {"orthogrid3d", false, &detail::run_orthogrid3d},
    {"print", true, &detail::run_print},
    {"traffic", true, &detail::run_traffic},
    {"report", true, &detail::run_report},
};

const ShowcaseBody* find_cpp_body(const std::string& mode) {
  for (const ShowcaseBody& b : kCppBodies) {
    if (mode == b.mode) {
      return &b;
    }
  }
  return nullptr;
}

}  // namespace

int plugin_showcase_body(Browser& browser, const std::string& mode) {
  if (const ShowcaseBody* body = find_cpp_body(mode)) {
    return body->run(browser);
  }
  (void)browser;
  detail::write_mark(detail::kPluginShowcaseMarkLeaf, "legacy-body-removed",
                     false);
  return 1;
}

int run_plugin_showcase(Browser& browser, const std::string& mode_in) {
  const std::string mode = normalize_plugin_showcase_id(mode_in);
  if (mode.empty()) {
    return 1;
  }
  if (const ShowcaseBody* body = find_cpp_body(mode)) {
    if (body->map2d_face) {
      base::set_switch("force-gdi-map-overlay", "1");
    }
    return body->run(browser);
  }

  const std::string suite = suite_id_for_mode(mode);
  base::set_switch("force-gdi-map-overlay", "1");
  if (!suite.empty() &&
      try_run_suite_script(browser, suite.c_str(),
                           detail::kPluginShowcaseMarkLeaf,
                           /*clear_marks=*/true)) {
    std::fprintf(stderr, "plugin-showcase: PASS mode=%s (script)\n",
                 mode.c_str());
    detail::detach_maps(browser);
    return 0;
  }
  std::fprintf(stderr,
               "plugin-showcase: script failed mode=%s (resolve or exec)\n",
               suite.empty() ? "(none)" : suite.c_str());
  detail::write_mark(detail::kPluginShowcaseMarkLeaf, "script-fail", false);
  detail::detach_maps(browser);
  return 1;
}

}  // namespace app
