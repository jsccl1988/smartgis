// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/capability/run_script.h"

#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/runtime/interact/apply.h"
#include "app/views/shell/runtime/capability/fill_host.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/capability/host.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace {

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

bool file_exists(const std::wstring& path) {
  return !path.empty() &&
         GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Recursively find |leaf| under |root| (depth-capped). Skips _shared only when
// looking for suite-owned scripts; shared orphans still match if unique.
bool find_named_under(const std::wstring& root,
                      const std::wstring& leaf,
                      std::wstring* out,
                      int depth) {
  if (!out || depth > 8) {
    return false;
  }
  const std::wstring pattern = root + L"\\*";
  WIN32_FIND_DATAW fd = {};
  HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) {
    return false;
  }
  bool found = false;
  do {
    if (fd.cFileName[0] == L'.' &&
        (fd.cFileName[1] == L'\0' ||
         (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) {
      continue;
    }
    const std::wstring child = root + L"\\" + fd.cFileName;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (find_named_under(child, leaf, out, depth + 1)) {
        found = true;
        break;
      }
      continue;
    }
    if (_wcsicmp(fd.cFileName, leaf.c_str()) == 0 && file_exists(child)) {
      *out = child;
      found = true;
      break;
    }
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  return found;
}

bool resolve_suite_script(const char* suite_id, std::wstring* out) {
  if (!out || !suite_id || !suite_id[0]) {
    return false;
  }
  if (const char* env = std::getenv("SMT_UI_INTERACT_SCRIPT")) {
    *out = widen_utf8(env);
    if (file_exists(*out)) {
      return true;
    }
  }

  std::wstring leaf = widen_utf8(suite_id);
  leaf += L".il";

  wchar_t sidecar[MAX_PATH] = {};
  if (detail::exe_sidecar_path(sidecar, MAX_PATH, leaf.c_str()) &&
      file_exists(sidecar)) {
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
    if (!file_exists(abs)) {
      continue;
    }
    // Direct layout: harness/<family>/<suite_id>/<suite_id>.il
    // Undotted ids live under shell/; dotted use the first segment as family
    // (pointcloud.load is under shell/ — recursive search covers that).
    std::wstring family = L"shell";
    const std::string_view id(suite_id);
    const size_t dot = id.find('.');
    if (dot != std::string_view::npos) {
      family = widen_utf8(std::string(id.substr(0, dot)).c_str());
    }
    const std::wstring sid_w = widen_utf8(suite_id);
    const std::wstring candidates[] = {
        abs + L"\\" + family + L"\\" + sid_w + L"\\" + leaf,
        abs + L"\\shell\\" + sid_w + L"\\" + leaf,
    };
    for (const std::wstring& direct : candidates) {
      if (file_exists(direct)) {
        *out = direct;
        return true;
      }
    }
    if (find_named_under(abs, leaf, out, 0)) {
      return true;
    }
  }

  // Sidecar next to exe (already tried) and flat leaf next to exe.
  const std::wstring beside = dir + L"\\" + leaf;
  if (file_exists(beside)) {
    *out = beside;
    return true;
  }
  return false;
}

}  // namespace

bool run_interact_script(Browser& browser,
                           const std::wstring& path,
                           const wchar_t* mark_leaf,
                           bool clear_marks) {
  content::CapabilityHost host;
  fill_host(browser, &host, mark_leaf);
  if (clear_marks && host.clear_marks) {
    host.clear_marks();
  }
  return try_apply_interact(host, path);
}

bool run_interact_script(Browser& browser,
                           const std::wstring& path,
                           const wchar_t* mark_leaf) {
  return run_interact_script(browser, path, mark_leaf, true);
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
  return run_interact_script(browser, path, mark_leaf, clear_marks);
}

bool try_run_suite_script(Browser& browser,
                          const char* suite_id,
                          const wchar_t* mark_leaf) {
  return try_run_suite_script(browser, suite_id, mark_leaf, true);
}

std::string run_interact_script_utf8(Browser& browser,
                                       const std::string& path_utf8,
                                       const wchar_t* mark_leaf) {
  const std::wstring path = widen_utf8(path_utf8.c_str());
  if (!file_exists(path)) {
    return std::string("error: missing script ") + path_utf8;
  }
  if (!run_interact_script(browser, path, mark_leaf)) {
    return std::string("error: script failed ") + path_utf8;
  }
  return std::string("ok: ") + path_utf8;
}

}  // namespace app
