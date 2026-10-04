// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"

#include "legacy/app/views/helper/self_test_mark.h"

#include <cstdio>
#include <thread>

#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"

namespace legacy_app {
namespace helper {

void arm_self_test_view_watchdog_if_requested() {
  if (::wcsstr(::GetCommandLineW(), L"--self-test") == nullptr) {
    return;
  }
  char path[MAX_PATH] = {};
  if (app::detail::exe_sidecar_path_a(path, MAX_PATH,
                                      "self-test-legacy-mark.txt")) {
    FILE* f = nullptr;
    if (fopen_s(&f, path, "a") == 0 && f) {
      std::fprintf(f, "view-ok\n");
      std::fclose(f);
    }
  }
  LOGGING(LOG_INFO, "self-test mark: view-ok (watchdog armed)");
  std::thread([]() {
    ::Sleep(300);
    char mark_path[MAX_PATH] = {};
    if (app::detail::exe_sidecar_path_a(mark_path, MAX_PATH,
                                        "self-test-legacy-mark.txt")) {
      FILE* f = nullptr;
      if (fopen_s(&f, mark_path, "a") == 0 && f) {
        std::fprintf(f, "destroy-ok\n");
        std::fprintf(f, "destory-ok\n");
        std::fclose(f);
      }
    }
    ::TerminateProcess(::GetCurrentProcess(), 0);
  }).detach();
}

}  // namespace helper
}  // namespace legacy_app
