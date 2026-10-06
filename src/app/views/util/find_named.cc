// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/util/find_named.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

// Closes a Win32 find handle when the walk leaves the scope.
struct FindHandle {
  FindHandle() = default;
  HANDLE handle = INVALID_HANDLE_VALUE;
  ~FindHandle() {
    if (handle != INVALID_HANDLE_VALUE) {
      FindClose(handle);
    }
  }
  FindHandle(const FindHandle&) = delete;
  FindHandle& operator=(const FindHandle&) = delete;
};

}  // namespace

bool find_named_under(const std::wstring& root,
                      const std::wstring& leaf,
                      std::wstring* out,
                      int depth) {
  if (!out || depth > 8) {
    return false;
  }
  const std::wstring pattern = root + L"\\*";
  WIN32_FIND_DATAW fd = {};
  FindHandle found_files;
  found_files.handle = FindFirstFileW(pattern.c_str(), &fd);
  if (found_files.handle == INVALID_HANDLE_VALUE) {
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
  } while (FindNextFileW(found_files.handle, &fd));
  return found;
}

}  // namespace detail
}  // namespace app
