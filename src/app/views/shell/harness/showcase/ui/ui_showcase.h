// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_SHOWCASE_UI_SHOWCASE_H_
#define APP_VIEWS_SHELL_SHOWCASE_UI_SHOWCASE_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

// Shell chrome capture path for --ui-showcase=shell|data|scene|catalog|interact.
// Writes ui-showcase-<mode>.bmp next to the exe (and ui-showcase-mark.txt).
int run_ui_showcase(Browser& browser, UiShowcaseMode mode);

}  // namespace app

#endif  // APP_VIEWS_SHELL_SHOWCASE_UI_SHOWCASE_H_
