// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/dem_bake_cache.h"

#include "vista/terrain/dem/cache/io.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
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

struct ViewSeedMemCache {
  std::mutex mu;
  std::string path;
  FileStamp stamp;
  int lod_key = 0;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  DemViewSeed seed;
  bool ready = false;
};

ViewSeedMemCache& view_seed_mem() {
  static ViewSeedMemCache c;
  return c;
}

#pragma pack(push, 1)
struct ViewSeedDiskHdr {
  char magic[8];
  uint64_t path_hash;
  uint64_t file_size;
  int64_t file_mtime;
  int32_t lod_key;
  double minx;
  double miny;
  double maxx;
  double maxy;
  float elev_cy;
  float min_x;
  float min_y;
  float min_z;
  float max_x;
  float max_y;
  float max_z;
  uint32_t tex_w;
  uint32_t tex_h;
  uint32_t xyz_n;
  uint32_t idx_n;
  uint32_t uv_n;
  uint32_t rgba_n;
};
#pragma pack(pop)

std::string view_seed_disk_path(const char* path, int lod_key, double minx,
                                double miny, double maxx, double maxy) {
  const std::string root = cache_root_dir();
  if (root.empty()) {
    return {};
  }
  const long long q0 = static_cast<long long>(minx * 1000.0);
  const long long q1 = static_cast<long long>(miny * 1000.0);
  const long long q2 = static_cast<long long>(maxx * 1000.0);
  const long long q3 = static_cast<long long>(maxy * 1000.0);
  char name[180];
  std::snprintf(name, sizeof(name),
                "%016llx_view_%d_%lld_%lld_%lld_%lld.bin",
                static_cast<unsigned long long>(path_hash64(path)), lod_key, q0,
                q1, q2, q3);
  return root + "\\" + name;
}

bool view_seed_key_match(const ViewSeedMemCache& c, const char* path,
                         const FileStamp& stamp, int lod_key, double minx,
                         double miny, double maxx, double maxy) {
  return c.ready && c.path == path && c.lod_key == lod_key &&
         c.stamp.size == stamp.size && c.stamp.mtime == stamp.mtime &&
         c.minx == minx && c.miny == miny && c.maxx == maxx && c.maxy == maxy &&
         !c.seed.xyz.empty() && !c.seed.indices.empty();
}

}  // namespace

bool dem_view_seed_cache_try_get(const char* path, int lod_key, double minx,
                                 double miny, double maxx, double maxy,
                                 DemViewSeed* out) {
  if (!path || !path[0] || !out || lod_key < 1) {
    return false;
  }
  warmup_dem_bake_cache();
  const FileStamp stamp = file_stamp(path);
  if (!stamp.ok) {
    return false;
  }
  {
    ViewSeedMemCache& c = view_seed_mem();
    std::lock_guard<std::mutex> lock(c.mu);
    if (view_seed_key_match(c, path, stamp, lod_key, minx, miny, maxx, maxy)) {
      *out = c.seed;
      return true;
    }
  }
  const std::string disk =
      view_seed_disk_path(path, lod_key, minx, miny, maxx, maxy);
  if (disk.empty()) {
    return false;
  }
  std::vector<uint8_t> blob;
  if (!read_all(disk, &blob) || blob.size() < sizeof(ViewSeedDiskHdr)) {
    return false;
  }
  ViewSeedDiskHdr hdr = {};
  std::memcpy(&hdr, blob.data(), sizeof(hdr));
  if (std::memcmp(hdr.magic, "SGVIEW1\0", 8) != 0 ||
      hdr.path_hash != path_hash64(path) || hdr.file_size != stamp.size ||
      hdr.file_mtime != stamp.mtime || hdr.lod_key != lod_key ||
      hdr.minx != minx || hdr.miny != miny || hdr.maxx != maxx ||
      hdr.maxy != maxy || hdr.xyz_n == 0 || hdr.idx_n == 0) {
    return false;
  }
  const size_t need =
      sizeof(ViewSeedDiskHdr) +
      static_cast<size_t>(hdr.xyz_n) * sizeof(float) +
      static_cast<size_t>(hdr.idx_n) * sizeof(uint32_t) +
      static_cast<size_t>(hdr.uv_n) * sizeof(float) +
      static_cast<size_t>(hdr.rgba_n);
  if (blob.size() < need) {
    return false;
  }
  DemViewSeed seed;
  seed.elev_cy = hdr.elev_cy;
  seed.min_x = hdr.min_x;
  seed.min_y = hdr.min_y;
  seed.min_z = hdr.min_z;
  seed.max_x = hdr.max_x;
  seed.max_y = hdr.max_y;
  seed.max_z = hdr.max_z;
  seed.tex_w = hdr.tex_w;
  seed.tex_h = hdr.tex_h;
  const uint8_t* p = blob.data() + sizeof(hdr);
  seed.xyz.resize(hdr.xyz_n);
  detail::copy_bytes_chunked(seed.xyz.data(), p, hdr.xyz_n * sizeof(float));
  p += hdr.xyz_n * sizeof(float);
  seed.indices.resize(hdr.idx_n);
  detail::copy_bytes_chunked(seed.indices.data(), p,
                             hdr.idx_n * sizeof(uint32_t));
  p += hdr.idx_n * sizeof(uint32_t);
  seed.uvs.resize(hdr.uv_n);
  if (hdr.uv_n > 0) {
    detail::copy_bytes_chunked(seed.uvs.data(), p, hdr.uv_n * sizeof(float));
    p += hdr.uv_n * sizeof(float);
  }
  seed.rgba.resize(hdr.rgba_n);
  if (hdr.rgba_n > 0) {
    detail::copy_bytes_chunked(seed.rgba.data(), p, hdr.rgba_n);
  }
  {
    ViewSeedMemCache& c = view_seed_mem();
    std::lock_guard<std::mutex> lock(c.mu);
    c.path = path;
    c.stamp = stamp;
    c.lod_key = lod_key;
    c.minx = minx;
    c.miny = miny;
    c.maxx = maxx;
    c.maxy = maxy;
    c.seed = seed;
    c.ready = true;
  }
  *out = std::move(seed);
  return true;
}

void dem_view_seed_cache_put(const char* path, int lod_key, double minx,
                             double miny, double maxx, double maxy,
                             const DemViewSeed& seed) {
  if (!path || !path[0] || seed.xyz.empty() || seed.indices.empty() ||
      lod_key < 1) {
    return;
  }
  warmup_dem_bake_cache();
  const FileStamp stamp = file_stamp(path);
  if (!stamp.ok) {
    return;
  }
  {
    ViewSeedMemCache& c = view_seed_mem();
    std::lock_guard<std::mutex> lock(c.mu);
    c.path = path;
    c.stamp = stamp;
    c.lod_key = lod_key;
    c.minx = minx;
    c.miny = miny;
    c.maxx = maxx;
    c.maxy = maxy;
    c.seed = seed;
    c.ready = true;
  }
  const std::string root = cache_root_dir();
  const std::string disk =
      view_seed_disk_path(path, lod_key, minx, miny, maxx, maxy);
  if (root.empty() || disk.empty() || !ensure_dir(root)) {
    return;
  }
  ViewSeedDiskHdr hdr = {};
  std::memcpy(hdr.magic, "SGVIEW1\0", 8);
  hdr.path_hash = path_hash64(path);
  hdr.file_size = stamp.size;
  hdr.file_mtime = stamp.mtime;
  hdr.lod_key = lod_key;
  hdr.minx = minx;
  hdr.miny = miny;
  hdr.maxx = maxx;
  hdr.maxy = maxy;
  hdr.elev_cy = seed.elev_cy;
  hdr.min_x = seed.min_x;
  hdr.min_y = seed.min_y;
  hdr.min_z = seed.min_z;
  hdr.max_x = seed.max_x;
  hdr.max_y = seed.max_y;
  hdr.max_z = seed.max_z;
  hdr.tex_w = seed.tex_w;
  hdr.tex_h = seed.tex_h;
  hdr.xyz_n = static_cast<uint32_t>(seed.xyz.size());
  hdr.idx_n = static_cast<uint32_t>(seed.indices.size());
  hdr.uv_n = static_cast<uint32_t>(seed.uvs.size());
  hdr.rgba_n = static_cast<uint32_t>(seed.rgba.size());
  const size_t bytes = sizeof(hdr) + seed.xyz.size() * sizeof(float) +
                       seed.indices.size() * sizeof(uint32_t) +
                       seed.uvs.size() * sizeof(float) + seed.rgba.size();
  std::vector<uint8_t> blob(bytes);
  std::memcpy(blob.data(), &hdr, sizeof(hdr));
  uint8_t* p = blob.data() + sizeof(hdr);
  detail::copy_bytes_chunked(p, seed.xyz.data(),
                             seed.xyz.size() * sizeof(float));
  p += seed.xyz.size() * sizeof(float);
  detail::copy_bytes_chunked(p, seed.indices.data(),
                             seed.indices.size() * sizeof(uint32_t));
  p += seed.indices.size() * sizeof(uint32_t);
  if (!seed.uvs.empty()) {
    detail::copy_bytes_chunked(p, seed.uvs.data(),
                               seed.uvs.size() * sizeof(float));
    p += seed.uvs.size() * sizeof(float);
  }
  if (!seed.rgba.empty()) {
    detail::copy_bytes_chunked(p, seed.rgba.data(), seed.rgba.size());
  }
  (void)write_all(disk, blob.data(), blob.size());
}

}  // namespace vista
