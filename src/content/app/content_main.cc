// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/app/content_main.h"

#include "content/public/process_type.h"

namespace content {

int ContentMain(const ContentMainParams& params) {
  switch (ProcessTypeFromCommandLine(params.argc, params.argv)) {
    case ProcessType::kRenderer:
      if (!params.renderer_main) {
        return 1;
      }
      return params.renderer_main(params);
    case ProcessType::kGpu:
      if (!params.gpu_main) {
        return 1;
      }
      return params.gpu_main(params);
    case ProcessType::kUtility:
      if (!params.utility_main) {
        return 1;
      }
      return params.utility_main(params);
    case ProcessType::kBrowser:
    default:
      if (!params.browser_main) {
        return 1;
      }
      return params.browser_main(params);
  }
}

}  // namespace content
