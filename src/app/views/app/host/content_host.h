// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_HOST_CONTENT_HOST_H_
#define APP_VIEWS_APP_HOST_CONTENT_HOST_H_

#include "app/views/app/process/browser_main.h"
#include "app/views/app/cmdline/views_launch_options.h"
#include "content/public/content_client.h"

namespace app {

// Browser-process embedder for SmartGisViews. Child --type= values are
// dispatched inside content::content_main.
struct ViewsContentHost : content::ContentClient {
  ViewsLaunchOptions options;

  int browser_main(const content::ContentMainParams& params) override {
    return run_browser_main(params, options);
  }
};

}  // namespace app

#endif  // APP_VIEWS_APP_HOST_CONTENT_HOST_H_
