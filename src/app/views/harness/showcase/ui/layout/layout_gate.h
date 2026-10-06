// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_UI_LAYOUT_GATE_H_
#define APP_VIEWS_HARNESS_SHOWCASE_UI_LAYOUT_GATE_H_

#include "app/views/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

namespace detail {

// Runs layout / overlap / shell anomaly checks. On failure (or UI_FORENSICS)
// dumps under out/ui_forensics/. Returns 0 on pass, else showcase exit code 30.
int run_ui_layout_gate(Browser& browser, UiShowcaseMode mode);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_UI_LAYOUT_GATE_H_
