// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/ui_designer/designer_io.h"

#include <sys/stat.h>

#include <cstdio>
#include <sstream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {

uint64_t file_mtime(const std::string& path) {
  struct _stat64 st = {};
  if (_stat64(path.c_str(), &st) != 0) {
    // UTF-8 path may need wide stat on Windows.
    const int n =
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (n > 0) {
      std::wstring w(static_cast<size_t>(n - 1), L'\0');
      MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w.data(), n);
      if (_wstat64(w.c_str(), &st) == 0) {
        return static_cast<uint64_t>(st.st_mtime);
      }
    }
    return 0;
  }
  return static_cast<uint64_t>(st.st_mtime);
}

std::string read_file_utf8(const std::string& path) {
  const int n =
      MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
  if (n <= 0) {
    return {};
  }
  std::wstring w(static_cast<size_t>(n - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w.data(), n);
  FILE* f = nullptr;
  if (_wfopen_s(&f, w.c_str(), L"rb") != 0 || !f) {
    return {};
  }
  std::ostringstream ss;
  char buf[4096];
  while (const size_t got = fread(buf, 1, sizeof(buf), f)) {
    ss.write(buf, static_cast<std::streamsize>(got));
  }
  fclose(f);
  return ss.str();
}

}  // namespace app
