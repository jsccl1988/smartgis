// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/frontend/load.h"

#include "app/views/util/charset.h"
#include "app/views/util/exe_sidecar_path.h"
#include "app/views/util/find_named.h"

#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "base/process/switches.h"

namespace app {
namespace {

// Authoring suffix. apply_script selects the frontend from the resolved path.
constexpr wchar_t kSuiteScriptSuffix[] = L".il";

}  // namespace

bool script_file_exists(const std::wstring& path) {
  return !path.empty() &&
         GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool resolve_suite_script(const char* suite_id, std::wstring* out) {
  if (!out || !suite_id || !suite_id[0]) {
    return false;
  }
  if (const char* env = base::switch_cstr("ui-interact-script")) {
    *out = detail::utf8_to_wide(env);
    if (script_file_exists(*out)) {
      return true;
    }
  }

  std::wstring leaf = detail::utf8_to_wide(suite_id);
  leaf += kSuiteScriptSuffix;

  wchar_t sidecar[MAX_PATH] = {};
  if (detail::exe_sidecar_path(sidecar, MAX_PATH, leaf.c_str()) &&
      script_file_exists(sidecar)) {
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

  // Prefer colocated suite dir: harness/<family>/<suite_id>/<suite_id>.il
  // Fall back to recursive search under harness/ (covers _shared orphans).
  const std::wstring harness_rels[] = {
      dir + L"\\..\\..\\testing\\tools\\harness",
      dir + L"\\..\\..\\..\\testing\\tools\\harness",
  };
  for (const std::wstring& harness : harness_rels) {
    wchar_t abs_buf[MAX_PATH] = {};
    const DWORD n =
        GetFullPathNameW(harness.c_str(), MAX_PATH, abs_buf, nullptr);
    if (n == 0 || n >= MAX_PATH) {
      continue;
    }
    const std::wstring abs(abs_buf);
    if (!script_file_exists(abs)) {
      continue;
    }
    // Undotted ids live under shell/; dotted ids use the first segment as
    // family (plugin.* / browser.* / ui.* / legacy.*).
    std::wstring family = L"shell";
    const std::string_view id(suite_id);
    const size_t dot = id.find('.');
    if (dot != std::string_view::npos) {
      family = detail::utf8_to_wide(std::string(id.substr(0, dot)).c_str());
    }
    const std::wstring sid_w = detail::utf8_to_wide(suite_id);
    const std::wstring candidates[] = {
        abs + L"\\" + family + L"\\" + sid_w + L"\\" + leaf,
        abs + L"\\plugin\\" + sid_w + L"\\" + leaf,
        abs + L"\\browser\\" + sid_w + L"\\" + leaf,
        abs + L"\\shell\\" + sid_w + L"\\" + leaf,
    };
    for (const std::wstring& direct : candidates) {
      if (script_file_exists(direct)) {
        *out = direct;
        return true;
      }
    }
    if (detail::find_named_under(abs, leaf, out, 0)) {
      return true;
    }
  }

  const std::wstring beside = dir + L"\\" + leaf;
  if (script_file_exists(beside)) {
    *out = beside;
    return true;
  }
  return false;
}

}  // namespace app
