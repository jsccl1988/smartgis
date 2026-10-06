// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_UI_SCENARIO_PANELS_H_
#define APP_VIEWS_HARNESS_SHOWCASE_UI_SCENARIO_PANELS_H_

#include "app/views/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

namespace detail {

// Mode-specific tab / catalog / inspector / scene panel orchestration before
// layout gate + BMP. Does not own product horizon widgets — only drives them.
void apply_ui_scenario_panels(Browser& browser, UiShowcaseMode mode);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_UI_SCENARIO_PANELS_H_
