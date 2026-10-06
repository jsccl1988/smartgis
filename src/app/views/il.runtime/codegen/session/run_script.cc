// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/codegen/session/run_script.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/link/bind_host.h"
#include "app/views/il.runtime/codegen/driver.h"
#include "app/views/il.runtime/codegen/session/interact_script.h"
#include "app/views/il.runtime/frontend/load.h"
#include "app/views/util/charset.h"
#include "content/browser/capability/host.h"

#include <cstdio>
#include <memory>
#include <string>

namespace app {
namespace {

// Session: link the runtime, install language hooks, then run the frontend.
// CapabilityHost is a large std::function bag — keep it off the stack so a
// skewed sizeof / deep apply_script frame cannot smash RTC cookies or the
// return address (failwithmessage / STATUS_ACCESS_VIOLATION under harness).
int run_linked_script(Browser& browser,
                      const std::wstring& path,
                      const wchar_t* mark_leaf,
                      bool clear_marks) {
  auto host = std::make_unique<content::CapabilityHost>();
  bind_host(browser, host.get(), mark_leaf);
  install_interact_frontend(browser, host.get());
  if (clear_marks && host->horizon.clear_marks) {
    host->horizon.clear_marks();
  }
  if (!apply_script(*host, path)) {
    return host->fail_rc != 0 ? host->fail_rc : 1;
  }
  return 0;
}

}  // namespace

bool run_execution_script(Browser& browser,
                          const std::wstring& path,
                          const wchar_t* mark_leaf,
                          bool clear_marks) {
  return run_linked_script(browser, path, mark_leaf, clear_marks) == 0;
}

bool run_execution_script(Browser& browser,
                          const std::wstring& path,
                          const wchar_t* mark_leaf) {
  return run_execution_script(browser, path, mark_leaf, true);
}

bool try_run_suite_script(Browser& browser,
                          const char* suite_id,
                          const wchar_t* mark_leaf,
                          bool clear_marks) {
  std::wstring path;
  if (!resolve_suite_script(suite_id, &path)) {
    std::fprintf(stderr, "run_script: resolve failed suite=%s\n",
                 suite_id ? suite_id : "");
    std::fflush(stderr);
    return false;
  }
  std::fwprintf(stderr, L"run_script: resolved %ls\n", path.c_str());
  std::fflush(stderr);
  return run_execution_script(browser, path, mark_leaf, clear_marks);
}

int run_suite_script_rc(Browser& browser,
                        const char* suite_id,
                        const wchar_t* mark_leaf,
                        bool clear_marks) {
  std::wstring path;
  if (!resolve_suite_script(suite_id, &path)) {
    std::fprintf(stderr, "run_script: resolve failed suite=%s\n",
                 suite_id ? suite_id : "");
    std::fflush(stderr);
    return 1;
  }
  std::fwprintf(stderr, L"run_script: resolved %ls\n", path.c_str());
  std::fflush(stderr);
  return run_linked_script(browser, path, mark_leaf, clear_marks);
}

bool try_run_suite_script(Browser& browser,
                          const char* suite_id,
                          const wchar_t* mark_leaf) {
  return try_run_suite_script(browser, suite_id, mark_leaf, true);
}

std::string run_execution_script_utf8(Browser& browser,
                                      const std::string& path_utf8,
                                      const wchar_t* mark_leaf) {
  const std::wstring path = detail::utf8_to_wide(path_utf8.c_str());
  if (!script_file_exists(path)) {
    return std::string("error: missing script ") + path_utf8;
  }
  if (!run_execution_script(browser, path, mark_leaf)) {
    return std::string("error: script failed ") + path_utf8;
  }
  return std::string("ok: ") + path_utf8;
}

}  // namespace app
