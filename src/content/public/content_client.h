// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_CONTENT_CLIENT_H_
#define CONTENT_PUBLIC_CONTENT_CLIENT_H_

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

// Browser-process hooks for one product embedder. Child process types are
// dispatched inside content_main; the product does not implement them.
class ContentClient {
 public:
  virtual ~ContentClient() = default;

  virtual int browser_main(const ContentMainParams& params) = 0;
};

// Starts this process. renderer, gpu, and utility are dispatched inside
// content. Browser (including an omitted --type=) calls client.browser_main.
int content_main(const ContentMainParams& params, ContentClient& client);

}  // namespace content

#endif  // CONTENT_PUBLIC_CONTENT_CLIENT_H_
