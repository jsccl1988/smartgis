// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/io.h"

#include <windows.h>

#include <cstdint>
#include <vector>

namespace app {
namespace detail {

bool file_exists_wide(const std::wstring& path) {
  return !path.empty() &&
         GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool read_utf8_file(const std::wstring& path,
                    std::string* out,
                    size_t max_bytes) {
  if (!out || path.empty() || max_bytes == 0) {
    return false;
  }
  const HANDLE file =
      CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
      static_cast<uint64_t>(size.QuadPart) > max_bytes) {
    CloseHandle(file);
    return false;
  }
  const DWORD n = static_cast<DWORD>(size.QuadPart);
  std::vector<char> buf(static_cast<size_t>(n));
  DWORD read = 0;
  const BOOL ok = ReadFile(file, buf.data(), n, &read, nullptr);
  CloseHandle(file);
  if (!ok || read != n) {
    return false;
  }
  out->assign(buf.data(), buf.size());
  return true;
}

}  // namespace detail
}  // namespace app
