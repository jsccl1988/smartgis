// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "host_window.h"

#include <objbase.h>
#include <shellapi.h>

#include "content/app/content_main.h"
#include "gpu/gpu.h"

namespace {

const wchar_t* parse_fixed_runtime(wchar_t* cmd) {
  const wchar_t kFlag[] = L"--webview-fixed=";
  wchar_t* found = wcsstr(cmd, kFlag);
  if (!found) {
    return nullptr;
  }
  return found + (sizeof(kFlag) / sizeof(wchar_t) - 1);
}

bool cmd_has_self_test() {
  const wchar_t* cmd = GetCommandLineW();
  return cmd && wcsstr(cmd, L"--self-test");
}

int BrowserMain(const content::ContentMainParams& params) {
  HINSTANCE instance = static_cast<HINSTANCE>(params.instance);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  const bool self_test = cmd_has_self_test();

  const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(com)) {
    return 1;
  }

  app::webview2::HostWindow host;
  if (!host.create(instance, parse_fixed_runtime(GetCommandLineW()))) {
    if (!self_test) {
      MessageBoxW(nullptr, L"Failed to create SmartGisWeb host window.",
                  L"SmartGIS Web", MB_OK | MB_ICONERROR);
    }
    CoUninitialize();
    return 1;
  }

  if (self_test) {
    CoUninitialize();
    return 0;
  }

  const int rc = host.run_loop();
  CoUninitialize();
  return rc;
}

int GpuMain(const content::ContentMainParams& params) {
  return gpu::render_main(params.argc, params.argv);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  content::ContentMainParams params;
  params.instance = instance;
  params.argc = argc;
  params.argv = argv;
  params.browser_main = &BrowserMain;
  params.gpu_main = &GpuMain;
  params.renderer_main = &GpuMain;
  const int rc = content::ContentMain(params);
  if (argv) {
    LocalFree(argv);
  }
  return rc;
}
