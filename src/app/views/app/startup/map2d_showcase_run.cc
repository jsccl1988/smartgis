// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/startup/map2d_showcase_run.h"

#include "app/views/app/startup/shell_runtime.h"

namespace app {

int run_map2d_china_showcase(Browser& browser) {
  return detail::run_map2d_showcase_cpp(browser, "china", "china-ok",
                                        L"map2d-showcase-china.bmp",
                                        "china_product");
}

int run_map2d_align_showcase(Browser& browser) {
  return detail::run_map2d_showcase_cpp(browser, "align", "align-ok",
                                        L"map2d-showcase-align.bmp",
                                        "china_product");
}

int run_map2d_orthogrid_showcase(Browser& browser) {
  return detail::run_map2d_showcase_cpp(browser, "orthogrid", "orthogrid-ok",
                                        L"map2d-showcase-orthogrid.bmp",
                                        "map2d_orthogrid");
}

}  // namespace app
