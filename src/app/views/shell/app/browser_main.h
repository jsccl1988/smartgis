// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_APP_BROWSER_MAIN_H_
#define APP_VIEWS_SHELL_APP_BROWSER_MAIN_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"
#include "content/app/content_main.h"

namespace app {

// Browser-process body for SmartGisViews: shell init, fields, showcase /
// self-test dispatch, or interactive run_loop.
int run_browser_main(const content::ContentMainParams& params,
                     const ViewsLaunchOptions& options);

}  // namespace app

#endif  // APP_VIEWS_SHELL_APP_BROWSER_MAIN_H_
