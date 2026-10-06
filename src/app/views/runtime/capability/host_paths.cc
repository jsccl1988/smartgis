// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/capability/host_paths.h"

#include "app/views/util/exe_sidecar_path.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

bool resolve_rel_under_exe(const wchar_t* const* rels,
                           size_t count,
                           std::string* out_utf8) {
  if (!out_utf8 || !rels || count == 0) {
    return false;
  }
  wchar_t base[MAX_PATH] = {};
  if (!detail::exe_dir_with_slash(base, MAX_PATH)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    wchar_t full[MAX_PATH] = {};
    if (wcscpy_s(full, base) != 0 || wcscat_s(full, rels[i]) != 0) {
      continue;
    }
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    wchar_t canon[MAX_PATH] = {};
    const wchar_t* use = full;
    if (GetFullPathNameW(full, MAX_PATH, canon, nullptr) != 0) {
      use = canon;
    }
    char utf8[MAX_PATH * 3] = {};
    if (!host_wide_to_utf8(use, utf8, sizeof(utf8))) {
      continue;
    }
    *out_utf8 = utf8;
    return true;
  }
  return false;
}

bool resolve_harness_leaf(const std::string& leaf_utf8, std::string* out_utf8) {
  if (!out_utf8 || leaf_utf8.empty()) {
    return false;
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
  const std::wstring leaf_w = host_utf8_to_wide(leaf_utf8);
  if (leaf_w.empty()) {
    return false;
  }
  const std::wstring harness_rels[] = {
      dir + L"\\..\\..\\testing\\tools\\harness",
      dir + L"\\..\\..\\..\\testing\\tools\\harness",
  };
  for (const std::wstring& harness : harness_rels) {
    wchar_t abs_buf[MAX_PATH] = {};
    const DWORD got =
        GetFullPathNameW(harness.c_str(), MAX_PATH, abs_buf, nullptr);
    if (got == 0 || got >= MAX_PATH) {
      continue;
    }
    std::wstring found;
    if (find_named_under(abs_buf, leaf_w, &found, 0) && !found.empty()) {
      char utf8[MAX_PATH * 3] = {};
      if (!host_wide_to_utf8(found.c_str(), utf8, sizeof(utf8))) {
        continue;
      }
      *out_utf8 = utf8;
      return true;
    }
  }
  return false;
}

}  // namespace

std::wstring host_utf8_to_wide(const std::string& utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int n =
      MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
  if (n <= 1) {
    return {};
  }
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
  w.resize(static_cast<size_t>(n - 1));
  return w;
}

bool host_wide_to_utf8(const wchar_t* wide, char* out, size_t out_cap) {
  if (!wide || !out || out_cap < 2) {
    return false;
  }
  return WideCharToMultiByte(CP_UTF8, 0, wide, -1, out,
                             static_cast<int>(out_cap), nullptr, nullptr) > 0;
}

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
    if (_wcsicmp(fd.cFileName, leaf.c_str()) == 0 &&
        GetFileAttributesW(child.c_str()) != INVALID_FILE_ATTRIBUTES) {
      *out = child;
      found = true;
      break;
    }
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  return found;
}

void bind_paths(content::CapabilityHost* out) {
  out->resolve_data = [](const std::string& kind, const std::string& leaf,
                         std::string* out_path) {
    if (!out_path || leaf.empty()) {
      return false;
    }
    if (kind == "harness") {
      return resolve_harness_leaf(leaf, out_path);
    }
    wchar_t a[MAX_PATH] = {};
    wchar_t bpath[MAX_PATH] = {};
    const std::wstring leaf_w = host_utf8_to_wide(leaf);
    if (leaf_w.empty()) {
      return false;
    }
    if (kind == "plugin") {
      if (swprintf_s(a, MAX_PATH, L"..\\data\\plugin\\%s", leaf_w.c_str()) <=
              0 ||
          swprintf_s(bpath, MAX_PATH, L"data\\plugin\\%s", leaf_w.c_str()) <=
              0) {
        return false;
      }
    } else if (kind == "data") {
      if (swprintf_s(a, MAX_PATH, L"..\\data\\%s", leaf_w.c_str()) <= 0 ||
          swprintf_s(bpath, MAX_PATH, L"data\\%s", leaf_w.c_str()) <= 0) {
        return false;
      }
    } else {
      return false;
    }
    const wchar_t* rels[] = {a, bpath};
    return resolve_rel_under_exe(rels, 2, out_path);
  };
  out->capture_path = [](const std::string& leaf, std::string* out_path) {
    if (!out_path || leaf.empty()) {
      return false;
    }
    const std::wstring leaf_w = host_utf8_to_wide(leaf);
    if (leaf_w.empty()) {
      return false;
    }
    wchar_t path_w[MAX_PATH] = {};
    if (!exe_capture_path(path_w, MAX_PATH, leaf_w.c_str())) {
      return false;
    }
    char utf8[MAX_PATH * 3] = {};
    if (!host_wide_to_utf8(path_w, utf8, sizeof(utf8))) {
      return false;
    }
    *out_path = utf8;
    return true;
  };
  out->sidecar_path = [](const std::string& rel, std::string* out_path) {
    if (!out_path || rel.empty()) {
      return false;
    }
    char path_a[MAX_PATH] = {};
    if (!exe_sidecar_path_a(path_a, MAX_PATH, rel.c_str())) {
      return false;
    }
    *out_path = path_a;
    return true;
  };
}

}  // namespace detail
}  // namespace app
