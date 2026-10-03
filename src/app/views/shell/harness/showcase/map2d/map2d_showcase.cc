// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/map2d_showcase.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/map2d/common/progress.h"
#include "app/views/shell/harness/showcase/map2d/present/present_run.h"
#include "app/views/shell/harness/showcase/map2d/seed/framing.h"
#include "app/views/shell/harness/showcase/map2d/seed/mode_seed.h"
#include "app/views/shell/runtime/capability/run_script.h"

#include <cstdio>
#include <cstdlib>

namespace {

using app::Map2dShowcaseMode;
using app::map2d_showcase_name;
using app::detail::detach_maps;
using app::detail::frame_map2d_showcase;
using app::detail::kMap2dShowcaseDefaultH;
using app::detail::kMap2dShowcaseDefaultW;
using app::detail::map2d_showcase_mark;
using app::detail::map2d_showcase_pixel_size;
using app::detail::run_map2d_present;
using app::detail::seed_map2d_mode;
using app::detail::stop_map_present_timers;

int run_map2d_showcase_impl(app::Browser& browser, Map2dShowcaseMode mode) {
  const char* name = map2d_showcase_name(mode);
  int showcase_w = kMap2dShowcaseDefaultW;
  int showcase_h = kMap2dShowcaseDefaultH;
  map2d_showcase_pixel_size(&showcase_w, &showcase_h);
  std::fprintf(stderr, "map2d-showcase mode=%s size=%dx%d\n", name, showcase_w,
               showcase_h);
  map2d_showcase_mark(name);

  browser.select_map_tab(0);
  map2d_showcase_mark("tab-map");
  app::pump_views_messages(400);
  map2d_showcase_mark("pumped");

  if (const int rc = seed_map2d_mode(browser, mode)) {
    detach_maps(browser);
    return rc;
  }

  if (const int rc =
          frame_map2d_showcase(browser, mode, showcase_w, showcase_h)) {
    detach_maps(browser);
    return rc;
  }
  app::pump_views_messages(100);
  map2d_showcase_mark("fit-ok");

  if (const int rc =
          run_map2d_present(browser, name, showcase_w, showcase_h)) {
    detach_maps(browser);
    return rc;
  }

  map2d_showcase_mark("pass");
  std::fprintf(stderr, "map2d-showcase: PASS mode=%s\n", name);
  // Timers only — detach_maps races TerminateProcess and surfaces as -1 after
  // a green BMP (peer browse / atmosphere).
  stop_map_present_timers(browser);
  app::pump_views_messages(100);
  return 0;
}

}  // namespace

namespace app {

int map2d_showcase_body(Browser& browser, Map2dShowcaseMode mode) {
  return run_map2d_showcase_impl(browser, mode);
}

int run_map2d_showcase(Browser& browser, Map2dShowcaseMode mode) {
  // Prefer the C++ body. The thin map2d.*.il → map2d_run path has hung /
  // surfaced EXIT=-1 after Browser::show with no marks/BMP (ANTLR resolve or
  // CapabilityHost fill). Body writes map2d-showcase-mark.txt + BMP directly.
  // Optional IL remains for interactive / SMT_UI_INTERACT_SCRIPT overrides.
  if (const char* force_il = std::getenv("SMT_MAP2D_SHOWCASE_IL");
      force_il && force_il[0] == '1' && force_il[1] == '\0') {
    const char* suite = nullptr;
    switch (mode) {
      case Map2dShowcaseMode::kChina:
        suite = "map2d.china";
        break;
      case Map2dShowcaseMode::kAlign:
        suite = "map2d.align";
        break;
      case Map2dShowcaseMode::kOrthogrid:
        suite = "map2d.orthogrid";
        break;
      case Map2dShowcaseMode::kNone:
        break;
    }
    if (suite &&
        try_run_suite_script(browser, suite, detail::kMap2dShowcaseMarkLeaf,
                             /*clear_marks=*/true)) {
      return 0;
    }
  }
  return map2d_showcase_body(browser, mode);
}

}  // namespace app
