// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_SEED_MODE_SEED_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_SEED_MODE_SEED_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

namespace detail {

// Loads document / style for --map2d-showcase=* (china / align / orthogrid).
// Writes progress marks. Returns 0 on success, else showcase exit code.
int seed_map2d_mode(Browser& browser, Map2dShowcaseMode mode);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_SEED_MODE_SEED_H_
