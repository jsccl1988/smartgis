// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/hwnd_register.h"

#include "plugin/product/map2d/scenario/framing.h"
#include "plugin/product/map2d/scenario/mode_seed.h"
#include "plugin/product/map2d/scenario/present_run.h"
#include "plugin/product/map2d/scenario/progress.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/shell.h"

#include <cstdio>

namespace plugin {
namespace {

int run_map2d_showcase(HarnessShell& browser, detail::ScenarioMode mode,
                       const char* name) {
  detail::bind_map2d_scenario_shell(&browser);
  browser.mark_named(kMarkMap2d, name, true);
  int showcase_w = detail::kMap2dDefaultW;
  int showcase_h = detail::kMap2dDefaultH;
  detail::map2d_pixel_size(&showcase_w, &showcase_h);
  std::fprintf(stderr, "map2d-showcase mode=%s size=%dx%d\n", name, showcase_w,
               showcase_h);
  detail::map2d_mark(name);

  browser.select_view_tab(0);
  detail::map2d_mark("tab-map");
  browser.pump(400);
  detail::map2d_mark("pumped");

  browser.stop_present_timers();
  if (const int rc = detail::seed_map2d_mode(browser, mode)) {
    browser.detach_views();
    return rc;
  }
  if (const int rc =
          detail::frame_map2d_showcase(browser, mode, showcase_w, showcase_h)) {
    browser.detach_views();
    return rc;
  }
  browser.resume_present_timers();
  browser.pump(100);
  detail::map2d_mark("fit-ok");
  // Same mark gate as browser.map2d.*.il after export_bmp framing.
  // Path used when --map2d-showcase-il is not 1 (C++ fallback).
  detail::map2d_mark("extent-ok");

  if (const int rc =
          detail::run_map2d_present(browser, name, showcase_w, showcase_h)) {
    browser.detach_views();
    return rc;
  }

  detail::map2d_mark("pass");
  std::fprintf(stderr, "map2d-showcase: PASS mode=%s\n", name);
  browser.stop_present_timers();
  browser.pump(100);
  return 0;
}

}  // namespace

int scenario_china(HarnessShell& browser) {
  return run_map2d_showcase(browser, detail::ScenarioMode::kChina, "china");
}

int scenario_align(HarnessShell& browser) {
  return run_map2d_showcase(browser, detail::ScenarioMode::kAlign, "align");
}

int scenario_orthogrid(HarnessShell& browser) {
  return run_map2d_showcase(browser, detail::ScenarioMode::kOrthogrid,
                            "orthogrid");
}

}  // namespace plugin
