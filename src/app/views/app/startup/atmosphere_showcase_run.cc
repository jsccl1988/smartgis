// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/startup/atmosphere_showcase_run.h"

#include "app/views/app/startup/shell_runtime.h"

namespace app {

int run_atmosphere_legacy_showcase(Browser& browser) {
  return detail::run_atmosphere_showcase_cpp(browser, "legacy",
                                             L"atmosphere-showcase-legacy.bmp");
}

int run_atmosphere_full_showcase(Browser& browser) {
  // Mirror testing/tools/harness/_shared/steps/scene/atmosphere.py::full —
  // planar looks in one process; suite BMP is the final full look.
  struct Step {
    const char* mode;
    const wchar_t* leaf;
    const char* mark;
  };
  static constexpr Step kSteps[] = {
      {"land", L"atmosphere-showcase-land.bmp", "bmp-land-ok"},
      {"ocean", L"atmosphere-showcase-ocean.bmp", "bmp-ocean-ok"},
      {"coast", L"atmosphere-showcase-coast.bmp", "bmp-coast-ok"},
      {"full", L"atmosphere-showcase-full.bmp", "bmp-full-ok"},
  };
  for (const Step& step : kSteps) {
    const int rc =
        detail::run_atmosphere_showcase_cpp(browser, step.mode, step.leaf);
    if (rc != 0) {
      return rc;
    }
    content::detail::write_mark(content::detail::kAtmosphereMarkLeaf, step.mark,
                                false);
  }
  // run_atmosphere_showcase_cpp already wrote bmp-ok / fit-box-ok /
  // camera-fly-ok / pass on the last (full) export.
  return 0;
}

}  // namespace app
