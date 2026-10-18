// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout.h"

#include <fstream>
#include <iterator>
#include <mutex>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace vista {
namespace {

std::string exe_dir() {
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  std::string dir(module, module + n);
  const size_t slash = dir.find_last_of("\\/");
  if (slash == std::string::npos) {
    return {};
  }
  dir.resize(slash + 1);
  return dir;
}

bool is_file(const std::string& path) {
  const DWORD attr = GetFileAttributesA(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES &&
         (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string read_file_utf8(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return {};
  }
  std::string data((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
      static_cast<unsigned char>(data[1]) == 0xBB &&
      static_cast<unsigned char>(data[2]) == 0xBF) {
    data.erase(0, 3);
  }
  return data;
}

// Shared out/ui/carto (exe under out/Debug|Release → ../ui), same pack as
// markup. Source-tree paths cover a binary launched from the repo root.
std::string resolve_carto_style_path(const char* leaf) {
  const std::string dir = exe_dir();
  const std::string candidates[] = {
      dir + "..\\ui\\carto\\" + leaf,
      dir + "../ui/carto/" + leaf,
      dir + "ui\\carto\\" + leaf,
      dir + "ui/carto/" + leaf,
      dir + "..\\..\\src\\ui\\resources\\carto\\" + leaf,
      dir + "../../src/ui/resources/carto/" + leaf,
      std::string("src\\ui\\resources\\carto\\") + leaf,
      std::string("src/ui/resources/carto/") + leaf,
  };
  for (const std::string& path : candidates) {
    if (is_file(path)) {
      return path;
    }
  }
  return {};
}

std::string load_style_json(const char* leaf) {
  const std::string path = resolve_carto_style_path(leaf);
  if (path.empty()) {
    return {};
  }
  return read_file_utf8(path);
}

}  // namespace

std::string default_carto_style_json() {
  static std::once_flag once;
  static std::string json;
  std::call_once(once, [] { json = load_style_json("default.style.json"); });
  return json;
}

std::string print_carto_style_json() {
  static std::once_flag once;
  static std::string json;
  std::call_once(once, [] { json = load_style_json("print.style.json"); });
  return json;
}

}  // namespace vista
