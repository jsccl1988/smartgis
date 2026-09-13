// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_APP_CONTENT_MAIN_H
#define CONTENT_APP_CONTENT_MAIN_H

// Process-type dispatcher for the chrome PE. Hosts fill the matching
// *_main pointer; this header stays free of windows.h and gpu/mojo.
namespace content {

struct ContentMainParams {
  void* instance = nullptr;  // HINSTANCE in the exe
  int argc = 0;
  wchar_t** argv = nullptr;
  int (*browser_main)(const ContentMainParams&) = nullptr;
  int (*renderer_main)(const ContentMainParams&) = nullptr;
  int (*gpu_main)(const ContentMainParams&) = nullptr;
  int (*utility_main)(const ContentMainParams&) = nullptr;
};

int ContentMain(const ContentMainParams& params);

}  // namespace content

#endif  // CONTENT_APP_CONTENT_MAIN_H
