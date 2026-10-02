// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_FRAME_EXTENT_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_FRAME_EXTENT_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

namespace detail {

// Frames ViewFrame to export pixels — not the live HWND client size.
// Returns 0 on success, else an exit code (caller owns detach_maps).
int frame_map2d_showcase(Browser& browser,
                         Map2dShowcaseMode mode,
                         int showcase_w,
                         int showcase_h);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_FRAME_EXTENT_H_
