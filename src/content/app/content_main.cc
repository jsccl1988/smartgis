// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/app/content_main.h"

#include "content/renderer/renderer_main.h"
#include "gpu/gpu.h"

namespace content {

int content_main(const ContentMainParams& params, ContentClient& client) {
  const ProcessType type = params.process_type_set
                               ? params.process_type
                               : ProcessTypeFromCommandLine(params.argc,
                                                            params.argv);
  switch (type) {
    case ProcessType::kRenderer:
      return RendererMain(params);
    case ProcessType::kGpu:
      // gpu_lib depends on content.dll, so this TU is linked into the shell
      // PE rather than the DLL. Callers do not include gpu/gpu.h.
      return gpu::GpuMain(params.argc, params.argv);
    case ProcessType::kUtility:
      // This PE does not run a utility process.
      return 1;
    case ProcessType::kBrowser:
    default:
      return client.browser_main(params);
  }
}

}  // namespace content
