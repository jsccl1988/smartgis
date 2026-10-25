// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_STARTUP_MAP2D_SHOWCASE_RUN_H_
#define APP_VIEWS_APP_STARTUP_MAP2D_SHOWCASE_RUN_H_

namespace app {

class Browser;

// C++ fallback for browser.map2d.{china,align,orthogrid} when the suite
// script cannot run (Debug has no CPython embed). Same seed → export_bmp →
// present_gpu → optional fps_bench path as the IL script; map-agnostic
// mechanics (mode only selects seed/frame leaf).
int run_map2d_china_showcase(Browser& browser);
int run_map2d_align_showcase(Browser& browser);
int run_map2d_orthogrid_showcase(Browser& browser);

}  // namespace app

#endif  // APP_VIEWS_APP_STARTUP_MAP2D_SHOWCASE_RUN_H_
