// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_PRESENT_CAPTURE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_PRESENT_CAPTURE_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

namespace detail {

// After layout gate: linger, keep Scene on 3D, composite FlyCube present
// pixels into the shell BMP, HUD FPS mark, chrome BMP write, stop timers.
// Returns 0 on success, else showcase exit code (54/55).
int run_ui_present_capture(Browser& browser, UiShowcaseMode mode);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_PRESENT_CAPTURE_H_
