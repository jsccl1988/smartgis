// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_GIS_CONTENTS_CLIENT_H_
#define CONTENT_PUBLIC_GIS_CONTENTS_CLIENT_H_

#include "content/app/process_type.h"

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

// Embedder callbacks content invokes after process dispatch (direction:
// content -> app). App implements browser_main. Do not put GIS document /
// viewport capability APIs here - those belong on GisContents.
class GisContentsClient {
 public:
  virtual ~GisContentsClient() = default;

  virtual int browser_main(const ContentMainParams& params) = 0;
};

// Starts this process. renderer, gpu, and utility are dispatched inside
// content. Browser (including an omitted --type=) calls client.browser_main.
int content_main(const ContentMainParams& params, GisContentsClient& client);

}  // namespace content

#endif  // CONTENT_PUBLIC_GIS_CONTENTS_CLIENT_H_
