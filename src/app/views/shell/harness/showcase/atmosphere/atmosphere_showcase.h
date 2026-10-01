// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_SHOWCASE_ATMOSPHERE_SHOWCASE_H_
#define APP_VIEWS_SHELL_SHOWCASE_ATMOSPHERE_SHOWCASE_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

// GPU/null atmosphere present path for --atmosphere-showcase=*.
// Prefers atmosphere.<mode>.il via CapabilityHost; falls back to body.
int run_atmosphere_showcase(Browser& browser, AtmosphereShowcaseMode mode);

// C++ body used by Host atmosphere_run and as script fallback.
int atmosphere_showcase_body(Browser& browser, AtmosphereShowcaseMode mode);

}  // namespace app

#endif  // APP_VIEWS_SHELL_SHOWCASE_ATMOSPHERE_SHOWCASE_H_
