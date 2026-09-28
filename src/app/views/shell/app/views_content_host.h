// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_APP_VIEWS_CONTENT_HOST_H_
#define APP_VIEWS_SHELL_APP_VIEWS_CONTENT_HOST_H_

#include "app/views/shell/app/browser_main.h"
#include "app/views/shell/app/cmdline/views_launch_options.h"
#include "content/app/content_main.h"
#include "content/renderer/renderer_main.h"
#include "gpu/gpu.h"

namespace app {

// Four process entries required by content::content_main_host.
// utility is not linked into this PE.
struct ViewsContentHost {
  ViewsLaunchOptions options;

  int browser_main(const content::ContentMainParams& params) const {
    return run_browser_main(params, options);
  }
  int renderer_main(const content::ContentMainParams& params) const {
    return content::RendererMain(params);
  }
  int gpu_main(const content::ContentMainParams& params) const {
    return gpu::GpuMain(params.argc, params.argv);
  }
  int utility_main(const content::ContentMainParams&) const { return 1; }
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_APP_VIEWS_CONTENT_HOST_H_
