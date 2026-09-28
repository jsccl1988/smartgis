// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_APP_CONTENT_MAIN_H
#define CONTENT_APP_CONTENT_MAIN_H

#include <concepts>

#include "content/app/process_type.h"

// Process-type dispatcher for one shell PE. The host satisfies
// content_main_host. This header stays free of windows.h and gpu headers.
namespace content {

struct ContentMainParams {
  void* instance = nullptr;  // HINSTANCE in the exe
  int argc = 0;
  wchar_t** argv = nullptr;
  // When process_type_set is true, content_main dispatches on process_type
  // (app CLI owns --type). Otherwise falls back to ProcessTypeFromCommandLine.
  ProcessType process_type = ProcessType::kBrowser;
  bool process_type_set = false;
};

// One entry per ProcessType. A role this PE does not run returns 1.
template <typename Host>
concept content_main_host = requires(Host& host, const ContentMainParams& params) {
  { host.browser_main(params) } -> std::same_as<int>;
  { host.renderer_main(params) } -> std::same_as<int>;
  { host.gpu_main(params) } -> std::same_as<int>;
  { host.utility_main(params) } -> std::same_as<int>;
};

template <content_main_host Host>
int content_main(const ContentMainParams& params, Host&& host) {
  const ProcessType type =
      params.process_type_set
          ? params.process_type
          : ProcessTypeFromCommandLine(params.argc, params.argv);
  switch (type) {
    case ProcessType::kRenderer:
      return host.renderer_main(params);
    case ProcessType::kGpu:
      return host.gpu_main(params);
    case ProcessType::kUtility:
      return host.utility_main(params);
    case ProcessType::kBrowser:
    default:
      return host.browser_main(params);
  }
}

}  // namespace content

#endif  // CONTENT_APP_CONTENT_MAIN_H
