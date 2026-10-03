// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_PRESENT_CAPTURE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_PRESENT_CAPTURE_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

namespace detail {

// After layout gate: optional linger, scene→Map flip for PrintWindow, shell
// repaint, HUD FPS mark, chrome BMP write, and stop present timers.
// Returns 0 on success, else showcase exit code (54/55).
int run_ui_present_capture(Browser& browser, UiShowcaseMode mode);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_PRESENT_CAPTURE_H_
