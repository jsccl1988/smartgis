// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/interact/interact_script.h"

#include "app/views/shell/runtime/interact/apply.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/ui/interact/interact_json.h"
#include "app/views/shell/runtime/capability/run_script.h"
#include "app/views/shell/util/exe_sidecar_path.h"

namespace app {
namespace {

void script_mark(const char* token) {
  detail::write_mark(detail::kUiShowcaseMarkLeaf, token, /*truncate=*/false);
}

bool env_is_os_driver() {
  const char* d = std::getenv("SMT_UI_INTERACT_DRIVER");
  return d && std::strcmp(d, "os") == 0;
}

int env_os_wait_ms() {
  if (const char* v = std::getenv("SMT_UI_INTERACT_OS_WAIT_MS")) {
    const int n = std::atoi(v);
    if (n > 0) {
      return n;
    }
  }
  return 6000;
}

std::wstring widen_utf8(const char* utf8) {
  if (!utf8 || !utf8[0]) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (n <= 1) {
    return {};
  }
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), n);
  out.resize(static_cast<size_t>(n - 1));
  return out;
}

bool resolve_script_path(std::wstring* out) {
  if (!out) {
    return false;
  }
  if (const char* env = std::getenv("SMT_UI_INTERACT_SCRIPT")) {
    *out = widen_utf8(env);
    if (!out->empty() &&
        GetFileAttributesW(out->c_str()) != INVALID_FILE_ATTRIBUTES) {
      return true;
    }
  }
  wchar_t sidecar[MAX_PATH] = {};
  if (detail::exe_sidecar_path(sidecar, MAX_PATH, L"ui.interact.il") &&
      GetFileAttributesW(sidecar) != INVALID_FILE_ATTRIBUTES) {
    *out = sidecar;
    return true;
  }
  wchar_t exe[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) {
    return false;
  }
  std::wstring dir(exe);
  const size_t slash = dir.find_last_of(L"\\/");
  if (slash == std::wstring::npos) {
    return false;
  }
  dir.resize(slash);
  const wchar_t* rels[] = {
      L"\\ui.interact.il",
      L"\\..\\..\\testing\\tools\\scripts\\ui.interact.il",
      L"\\..\\..\\..\\testing\\tools\\scripts\\ui.interact.il",
  };
  for (const wchar_t* rel : rels) {
    std::wstring cand = dir + rel;
    if (GetFileAttributesW(cand.c_str()) != INVALID_FILE_ATTRIBUTES) {
      *out = cand;
      return true;
    }
  }
  return false;
}

}  // namespace

bool interact_script_os_driver() {
  return env_is_os_driver();
}

bool try_apply_interact_script(Browser& browser) {
  if (env_is_os_driver()) {
    script_mark("os-driver");
    // Settle Map tab chrome before outer injector runs — OS script skips
    // select_map_tab / catalog / inspector (inproc-only), but BMP gates still
    // need a painted tab accent.
    browser.select_map_tab(0);
    pump_views_messages(400);
    const int wait_ms = env_os_wait_ms();
    std::fprintf(stderr, "interact-script: OS driver wait %d ms\n", wait_ms);
    pump_views_messages(static_cast<DWORD>(wait_ms));
    script_mark("os-wait-done");
    return true;
  }
  std::wstring path;
  if (!resolve_script_path(&path)) {
    return false;
  }
  std::fwprintf(stderr, L"interact-script: %ls\n", path.c_str());
  if (is_interact_path(path)) {
    return run_interact_script(browser, path, L"ui-showcase-mark.txt");
  }
  return detail::apply_ui_interact_json(browser, path);
}

}  // namespace app
