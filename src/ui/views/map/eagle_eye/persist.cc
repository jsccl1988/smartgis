// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/eagle_eye/persist.h"

#include <filesystem>
#include <fstream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <shlobj.h>
#include <windows.h>

namespace ui {
namespace views {
namespace detail {
namespace {

std::filesystem::path eagle_eye_persist_path() {
  wchar_t* folder = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder)) ||
      !folder) {
    return {};
  }
  std::filesystem::path path(folder);
  CoTaskMemFree(folder);
  path /= L"SmartGIS";
  path /= L"eagle_eye_visible.txt";
  return path;
}

}  // namespace

bool eagle_eye_load_visible() {
  const auto path = eagle_eye_persist_path();
  if (path.empty()) {
    return true;
  }
  std::ifstream in(path);
  if (!in) {
    return true;
  }
  std::string line;
  if (!std::getline(in, line)) {
    return true;
  }
  return !(line == "0" || line == "false" || line == "off");
}

void eagle_eye_persist_visible(bool visible) {
  const auto path = eagle_eye_persist_path();
  if (path.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    return;
  }
  out << (visible ? "1" : "0");
}

}  // namespace detail
}  // namespace views
}  // namespace ui
