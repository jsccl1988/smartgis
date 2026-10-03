// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_CHINA_SEED_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_CHINA_SEED_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

namespace detail {

// Seeds China PLP into Map Edit (and refreshes Catalog) after show, before
// capture. Skips scene mode (GDI placeholder / FlyCube path). Writes marks.
void ensure_ui_showcase_china_map(Browser& browser, UiShowcaseMode mode);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_CHINA_SEED_H_
