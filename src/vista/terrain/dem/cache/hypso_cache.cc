// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/dem_bake_cache.h"

#include "vista/terrain/dem/cache/io.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace vista {
namespace {

using detail::FileStamp;
using detail::cache_root_dir;
using detail::ensure_dir;
using detail::file_stamp;
using detail::path_hash64;
using detail::read_all;
using detail::warmup_dem_bake_cache;
using detail::write_all;

struct HypsoMemCache {
  std::mutex mu;
  std::string path;
  FileStamp stamp;
  int max_edge = 0;
  int w = 0;
  int h = 0;
  std::vector<uint8_t> rgba;
  bool ready = false;
};

HypsoMemCache& hypso_mem() {
  static HypsoMemCache c;
  return c;
}

#pragma pack(push, 1)
struct HypsoDiskHdr {
  char magic[8];
  uint64_t path_hash;
  uint64_t file_size;
  int64_t file_mtime;
  int32_t max_edge;
  int32_t w;
  int32_t h;
};
#pragma pack(pop)

std::string hypso_disk_path(const char* path, int max_edge) {
  const std::string root = cache_root_dir();
  if (root.empty()) {
    return {};
  }
  char name[80];
  std::snprintf(name, sizeof(name), "%016llx_hypso_%d.bin",
                static_cast<unsigned long long>(path_hash64(path)), max_edge);
  return root + "\\" + name;
}

}  // namespace

bool dem_hypso_cache_try_get(const char* path, int max_edge,
                             std::vector<uint8_t>* rgba, int* out_w,
                             int* out_h) {
  if (!path || !path[0] || !rgba || max_edge < 2) {
    return false;
  }
  warmup_dem_bake_cache();
  const FileStamp stamp = file_stamp(path);
  if (!stamp.ok) {
    return false;
  }

  {
    HypsoMemCache& c = hypso_mem();
    std::lock_guard<std::mutex> lock(c.mu);
    if (c.ready && c.path == path && c.max_edge == max_edge &&
        c.stamp.size == stamp.size && c.stamp.mtime == stamp.mtime &&
        !c.rgba.empty() && c.w > 0 && c.h > 0) {
      *rgba = c.rgba;
      if (out_w) {
        *out_w = c.w;
      }
      if (out_h) {
        *out_h = c.h;
      }
      return true;
    }
  }

  const std::string disk = hypso_disk_path(path, max_edge);
  if (disk.empty()) {
    return false;
  }
  std::vector<uint8_t> blob;
  if (!read_all(disk, &blob) || blob.size() < sizeof(HypsoDiskHdr)) {
    return false;
  }
  HypsoDiskHdr hdr = {};
  std::memcpy(&hdr, blob.data(), sizeof(hdr));
  if (std::memcmp(hdr.magic, "SGHYP1\0\0", 8) != 0 ||
      hdr.path_hash != path_hash64(path) || hdr.file_size != stamp.size ||
      hdr.file_mtime != stamp.mtime || hdr.max_edge != max_edge || hdr.w < 2 ||
      hdr.h < 2) {
    return false;
  }
  const size_t need = sizeof(HypsoDiskHdr) +
                      static_cast<size_t>(hdr.w) * static_cast<size_t>(hdr.h) *
                          4u;
  if (blob.size() < need) {
    return false;
  }
  const size_t pix =
      static_cast<size_t>(hdr.w) * static_cast<size_t>(hdr.h) * 4u;
  rgba->resize(pix);
  detail::copy_bytes_chunked(rgba->data(), blob.data() + sizeof(hdr), pix);
  if (out_w) {
    *out_w = hdr.w;
  }
  if (out_h) {
    *out_h = hdr.h;
  }

  HypsoMemCache& c = hypso_mem();
  std::lock_guard<std::mutex> lock(c.mu);
  c.path = path;
  c.stamp = stamp;
  c.max_edge = max_edge;
  c.w = hdr.w;
  c.h = hdr.h;
  c.rgba = *rgba;
  c.ready = true;
  return true;
}

void dem_hypso_cache_put(const char* path, int max_edge,
                         const std::vector<uint8_t>& rgba, int w, int h) {
  if (!path || !path[0] || rgba.empty() || w < 2 || h < 2 || max_edge < 2) {
    return;
  }
  if (rgba.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 4u) {
    return;
  }
  warmup_dem_bake_cache();
  const FileStamp stamp = file_stamp(path);
  if (!stamp.ok) {
    return;
  }

  {
    HypsoMemCache& c = hypso_mem();
    std::lock_guard<std::mutex> lock(c.mu);
    c.path = path;
    c.stamp = stamp;
    c.max_edge = max_edge;
    c.w = w;
    c.h = h;
    c.rgba = rgba;
    c.ready = true;
  }

  const std::string root = cache_root_dir();
  const std::string disk = hypso_disk_path(path, max_edge);
  if (root.empty() || disk.empty() || !ensure_dir(root)) {
    return;
  }
  HypsoDiskHdr hdr = {};
  std::memcpy(hdr.magic, "SGHYP1\0\0", 8);
  hdr.path_hash = path_hash64(path);
  hdr.file_size = stamp.size;
  hdr.file_mtime = stamp.mtime;
  hdr.max_edge = max_edge;
  hdr.w = w;
  hdr.h = h;
  const size_t pix = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
  std::vector<uint8_t> blob(sizeof(hdr) + pix);
  std::memcpy(blob.data(), &hdr, sizeof(hdr));
  detail::copy_bytes_chunked(blob.data() + sizeof(hdr), rgba.data(), pix);
  (void)write_all(disk, blob.data(), blob.size());
}

}  // namespace vista
