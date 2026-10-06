// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/cache/io.h"

#include "base/files/mapped_file.h"
#include "base/process/switches.h"
#include "vista/terrain/dem/cache/io_file_loader.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace vista {
namespace detail {
namespace {

constexpr DWORD kIoWriteChunk = 1u << 20;  // 1 MiB sequential store.

std::string& cache_root_store() {
  static std::string root;
  return root;
}

std::once_flag& warmup_once() {
  static std::once_flag once;
  return once;
}

std::string resolve_cache_root_dir() {
  if (const char* e = base::switch_cstr("dem-bake-cache")) {
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
  // out/Debug → out/data/cache/dem_bake
  return dir + "\\..\\data\\cache\\dem_bake";
}

}  // namespace

FileStamp file_stamp(const char* path) {
  FileStamp s;
  if (!path || !path[0]) {
    return s;
  }
  WIN32_FILE_ATTRIBUTE_DATA fad = {};
  if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
    return s;
  }
  ULARGE_INTEGER sz;
  sz.HighPart = fad.nFileSizeHigh;
  sz.LowPart = fad.nFileSizeLow;
  s.size = sz.QuadPart;
  ULARGE_INTEGER mt;
  mt.HighPart = fad.ftLastWriteTime.dwHighDateTime;
  mt.LowPart = fad.ftLastWriteTime.dwLowDateTime;
  s.mtime = static_cast<int64_t>(mt.QuadPart);
  s.ok = true;
  return s;
}

uint64_t path_hash64(const char* path) {
  uint64_t h = 14695981039346656037ull;
  if (!path) {
    return h;
  }
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path);
       *p; ++p) {
    unsigned char c = *p;
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<unsigned char>(c - 'A' + 'a');
    }
    if (c == '/') {
      c = '\\';
    }
    h ^= c;
    h *= 1099511628211ull;
  }
  return h;
}

std::string cache_root_dir() {
  std::string& stored = cache_root_store();
  if (!stored.empty()) {
    return stored;
  }
  std::string resolved = resolve_cache_root_dir();
  if (!resolved.empty()) {
    stored = resolved;
  }
  return resolved;
}

bool ensure_dir(const std::string& dir) {
  if (dir.empty()) {
    return false;
  }
  std::string cur;
  for (size_t i = 0; i < dir.size(); ++i) {
    const char c = dir[i];
    cur.push_back(c);
    const bool sep = (c == '\\' || c == '/');
    const bool last = (i + 1 == dir.size());
    if (!sep && !last) {
      continue;
    }
    if (cur.size() <= 3 && cur.find(':') != std::string::npos) {
      continue;
    }
    CreateDirectoryA(cur.c_str(), nullptr);
  }
  return true;
}

void warmup_dem_bake_cache() {
  std::call_once(warmup_once(), [] {
    const std::string root = cache_root_dir();
    if (!root.empty()) {
      (void)ensure_dir(root);
    }
  });
}

bool read_all_mapped_chunked(const std::string& path, std::vector<uint8_t>* out) {
  if (!out || path.empty()) {
    return false;
  }
  warmup_dem_bake_cache();
  base::MappedFile map;
  if (!map.open(path.c_str(), base::MappedFileAdvice::kSequential)) {
    return false;
  }
  if (map.size() == 0 || !map.data()) {
    out->clear();
    return false;
  }
  out->resize(map.size());
  copy_bytes_chunked(out->data(), map.data(), map.size());
  return true;
}

bool read_all_file_loader(const std::string& path, std::vector<uint8_t>* out,
                          size_t parallel_num, size_t block_size, bool warmup) {
  if (!out || path.empty()) {
    return false;
  }
  warmup_dem_bake_cache();
  return load_file_via_loader(path.c_str(), out, parallel_num, block_size,
                              warmup);
}

bool read_all(const std::string& path, std::vector<uint8_t>* out,
              DemIoReadMode mode) {
  switch (mode) {
    case DemIoReadMode::kFileLoader:
      return read_all_file_loader(path, out);
    case DemIoReadMode::kMappedChunked:
    default:
      return read_all_mapped_chunked(path, out);
  }
}

bool read_all(const std::string& path, std::vector<uint8_t>* out) {
  return read_all_mapped_chunked(path, out);
}

bool write_all(const std::string& path, const void* data, size_t bytes) {
  if (!data || bytes == 0 || path.empty()) {
    return false;
  }
  warmup_dem_bake_cache();
  const HANDLE h =
      CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                  FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    return false;
  }
  const auto* p = static_cast<const uint8_t*>(data);
  size_t remaining = bytes;
  while (remaining > 0) {
    const DWORD want = static_cast<DWORD>(
        (std::min)(remaining, static_cast<size_t>(kIoWriteChunk)));
    DWORD wrote = 0;
    if (!WriteFile(h, p, want, &wrote, nullptr) || wrote == 0) {
      CloseHandle(h);
      return false;
    }
    p += wrote;
    remaining -= wrote;
  }
  CloseHandle(h);
  return true;
}

}  // namespace detail
}  // namespace vista
