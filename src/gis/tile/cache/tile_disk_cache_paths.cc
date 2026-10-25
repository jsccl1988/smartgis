// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/tile/cache/tile_disk_cache_paths.h"

#include "base/process/switches.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace gis {
namespace tile {
namespace {

uint64_t fnv1a64(const unsigned char* data, size_t len) {
  uint64_t h = 14695981039346656037ull;
  for (size_t i = 0; i < len; ++i) {
    h ^= static_cast<uint64_t>(data[i]);
    h *= 1099511628211ull;
  }
  return h;
}

std::string hex_u64(uint64_t v) {
  static const char* k_hex = "0123456789abcdef";
  std::string out(16, '0');
  for (int i = 15; i >= 0; --i) {
    out[static_cast<size_t>(i)] = k_hex[v & 0xf];
    v >>= 4;
  }
  return out;
}

}  // namespace

uint64_t url_template_hash64(const std::string& url_template) {
  return fnv1a64(reinterpret_cast<const unsigned char*>(url_template.data()),
                 url_template.size());
}

std::string default_xyz_tile_cache_root() {
  if (const char* e = base::switch_cstr("tile-disk-cache")) {
    if (e[0]) {
      return std::string(e);
    }
  }
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
  dir.resize(slash);
  return dir + "\\..\\data\\cache\\xyz_tiles";
}

std::string disk_cache_dir_for_template(const std::string& url_template) {
  if (url_template.empty()) {
    return {};
  }
  const std::string root = default_xyz_tile_cache_root();
  if (root.empty()) {
    return {};
  }
  return root + "\\" + hex_u64(url_template_hash64(url_template));
}

}  // namespace tile
}  // namespace gis
