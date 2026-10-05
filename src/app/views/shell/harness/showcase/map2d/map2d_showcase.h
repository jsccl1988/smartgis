// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_SHOWCASE_MAP2D_SHOWCASE_H_
#define APP_VIEWS_SHELL_SHOWCASE_MAP2D_SHOWCASE_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

// China / align / orthogrid 2D present + BMP sidecar for --map2d-showcase=*.
// orthogrid: four boundary curves → Laplace(+heat) → export BMP via mesh writer.
// Runs the C++ body by default (stable marks/BMP). Set MAP2D_SHOWCASE_IL=1
// to prefer map2d.<mode>.il via CapabilityHost.
int run_map2d_showcase(Browser& browser, Map2dShowcaseMode mode);

// C++ body used by Host map2d_run and as script fallback.
int map2d_showcase_body(Browser& browser, Map2dShowcaseMode mode);

}  // namespace app

#endif  // APP_VIEWS_SHELL_SHOWCASE_MAP2D_SHOWCASE_H_
