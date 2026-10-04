// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/ui_designer/app/app.h"

#include <string>

#include <windows.h>
#include <shellapi.h>

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::string initial;
  if (argc >= 2 && argv) {
    char buf[MAX_PATH * 3] = {};
    WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, buf, static_cast<int>(sizeof(buf)),
                        nullptr, nullptr);
    initial = buf;
  }
  if (argv) {
    LocalFree(argv);
  }
  return app::run_ui_designer(initial);
}
