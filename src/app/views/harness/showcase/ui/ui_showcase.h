// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHOWCASE_UI_SHOWCASE_H_
#define APP_VIEWS_SHOWCASE_UI_SHOWCASE_H_

#include "app/views/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

// Shell horizon capture path for --ui-showcase=shell|data|scene|catalog|interact.
// Composes seed/ + present/ + layout/ + capture/ + session/ under showcase/ui/
// (not product shell/ui horizon). Writes ui-showcase-<mode>.bmp under
// out/<config>/captures/ui/ (and ui-showcase-mark.txt).
int run_ui_showcase(Browser& browser, UiShowcaseMode mode);

}  // namespace app

#endif  // APP_VIEWS_SHOWCASE_UI_SHOWCASE_H_
