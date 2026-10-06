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

struct MeshMemCache {
  std::mutex mu;
  std::string path;
  FileStamp stamp;
  int max_edge = 0;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  bool windowed = false;
  bool apply_land_mask = true;
  std::vector<float> xyz;
  std::vector<uint32_t> indices;
  std::vector<float> uvs;
  bool ready = false;
};

MeshMemCache& mesh_mem() {
  static MeshMemCache c;
  return c;
}

#pragma pack(push, 1)
struct MeshDiskHdr {
  char magic[8];
  uint64_t path_hash;
  uint64_t file_size;
  int64_t file_mtime;
  int32_t max_edge;
  int32_t windowed;
  int32_t apply_land_mask;
  double minx;
  double miny;
  double maxx;
  double maxy;
  uint32_t xyz_n;
  uint32_t idx_n;
  uint32_t uv_n;
};
#pragma pack(pop)

std::string mesh_disk_path(const char* path, int max_edge, bool windowed,
                           bool apply_land_mask, double minx, double miny,
                           double maxx, double maxy) {
  const std::string root = cache_root_dir();
  if (root.empty()) {
    return {};
  }
  // Quantize extents so float noise does not fragment cache keys.
  const long long q0 = static_cast<long long>(minx * 1000.0);
  const long long q1 = static_cast<long long>(miny * 1000.0);
  const long long q2 = static_cast<long long>(maxx * 1000.0);
  const long long q3 = static_cast<long long>(maxy * 1000.0);
  char name[160];
  std::snprintf(name, sizeof(name),
                "%016llx_mesh_%d_%d%d_%lld_%lld_%lld_%lld.bin",
                static_cast<unsigned long long>(path_hash64(path)), max_edge,
                windowed ? 1 : 0, apply_land_mask ? 1 : 0, q0, q1, q2, q3);
  return root + "\\" + name;
}

}  // namespace

bool dem_mesh_cache_try_get(const char* path, int max_edge, double minx,
                            double miny, double maxx, double maxy, bool windowed,
                            bool apply_land_mask, std::vector<float>* xyz,
                            std::vector<uint32_t>* indices,
                            std::vector<float>* uvs) {
  if (!path || !path[0] || !xyz || !indices || max_edge < 2) {
    return false;
  }
  warmup_dem_bake_cache();
  const FileStamp stamp = file_stamp(path);
  if (!stamp.ok) {
    return false;
  }
  {
    MeshMemCache& c = mesh_mem();
    std::lock_guard<std::mutex> lock(c.mu);
    if (c.ready && c.path == path && c.max_edge == max_edge &&
        c.windowed == windowed && c.apply_land_mask == apply_land_mask &&
        c.stamp.size == stamp.size && c.stamp.mtime == stamp.mtime &&
        (!windowed || (c.minx == minx && c.miny == miny && c.maxx == maxx &&
                       c.maxy == maxy)) &&
        !c.xyz.empty() && !c.indices.empty()) {
      *xyz = c.xyz;
      *indices = c.indices;
      if (uvs) {
        *uvs = c.uvs;
      }
      return true;
    }
  }
  const std::string disk = mesh_disk_path(path, max_edge, windowed,
                                          apply_land_mask, minx, miny, maxx,
                                          maxy);
  if (disk.empty()) {
    return false;
  }
  std::vector<uint8_t> blob;
  if (!read_all(disk, &blob) || blob.size() < sizeof(MeshDiskHdr)) {
    return false;
  }
  MeshDiskHdr hdr = {};
  std::memcpy(&hdr, blob.data(), sizeof(hdr));
  if (std::memcmp(hdr.magic, "SGMESH1\0", 8) != 0 ||
      hdr.path_hash != path_hash64(path) || hdr.file_size != stamp.size ||
      hdr.file_mtime != stamp.mtime || hdr.max_edge != max_edge ||
      (hdr.windowed != 0) != windowed ||
      (hdr.apply_land_mask != 0) != apply_land_mask) {
    return false;
  }
  if (windowed && (hdr.minx != minx || hdr.miny != miny || hdr.maxx != maxx ||
                   hdr.maxy != maxy)) {
    return false;
  }
  const size_t need = sizeof(MeshDiskHdr) +
                      static_cast<size_t>(hdr.xyz_n) * sizeof(float) +
                      static_cast<size_t>(hdr.idx_n) * sizeof(uint32_t) +
                      static_cast<size_t>(hdr.uv_n) * sizeof(float);
  if (blob.size() < need || hdr.xyz_n == 0 || hdr.idx_n == 0) {
    return false;
  }
  const uint8_t* p = blob.data() + sizeof(hdr);
  xyz->resize(hdr.xyz_n);
  detail::copy_bytes_chunked(xyz->data(), p, hdr.xyz_n * sizeof(float));
  p += hdr.xyz_n * sizeof(float);
  indices->resize(hdr.idx_n);
  detail::copy_bytes_chunked(indices->data(), p, hdr.idx_n * sizeof(uint32_t));
  p += hdr.idx_n * sizeof(uint32_t);
  if (uvs) {
    uvs->resize(hdr.uv_n);
    if (hdr.uv_n > 0) {
      detail::copy_bytes_chunked(uvs->data(), p, hdr.uv_n * sizeof(float));
    }
  }
  MeshMemCache& c = mesh_mem();
  std::lock_guard<std::mutex> lock(c.mu);
  c.path = path;
  c.stamp = stamp;
  c.max_edge = max_edge;
  c.minx = minx;
  c.miny = miny;
  c.maxx = maxx;
  c.maxy = maxy;
  c.windowed = windowed;
  c.apply_land_mask = apply_land_mask;
  c.xyz = *xyz;
  c.indices = *indices;
  c.uvs = uvs ? *uvs : std::vector<float>{};
  c.ready = true;
  return true;
}

void dem_mesh_cache_put(const char* path, int max_edge, double minx,
                        double miny, double maxx, double maxy, bool windowed,
                        bool apply_land_mask, const std::vector<float>& xyz,
                        const std::vector<uint32_t>& indices,
                        const std::vector<float>& uvs) {
  if (!path || !path[0] || xyz.empty() || indices.empty() || max_edge < 2) {
    return;
  }
  warmup_dem_bake_cache();
  const FileStamp stamp = file_stamp(path);
  if (!stamp.ok) {
    return;
  }
  {
    MeshMemCache& c = mesh_mem();
    std::lock_guard<std::mutex> lock(c.mu);
    c.path = path;
    c.stamp = stamp;
    c.max_edge = max_edge;
    c.minx = minx;
    c.miny = miny;
    c.maxx = maxx;
    c.maxy = maxy;
    c.windowed = windowed;
    c.apply_land_mask = apply_land_mask;
    c.xyz = xyz;
    c.indices = indices;
    c.uvs = uvs;
    c.ready = true;
  }
  const std::string root = cache_root_dir();
  const std::string disk = mesh_disk_path(path, max_edge, windowed,
                                          apply_land_mask, minx, miny, maxx,
                                          maxy);
  if (root.empty() || disk.empty() || !ensure_dir(root)) {
    return;
  }
  MeshDiskHdr hdr = {};
  std::memcpy(hdr.magic, "SGMESH1\0", 8);
  hdr.path_hash = path_hash64(path);
  hdr.file_size = stamp.size;
  hdr.file_mtime = stamp.mtime;
  hdr.max_edge = max_edge;
  hdr.windowed = windowed ? 1 : 0;
  hdr.apply_land_mask = apply_land_mask ? 1 : 0;
  hdr.minx = minx;
  hdr.miny = miny;
  hdr.maxx = maxx;
  hdr.maxy = maxy;
  hdr.xyz_n = static_cast<uint32_t>(xyz.size());
  hdr.idx_n = static_cast<uint32_t>(indices.size());
  hdr.uv_n = static_cast<uint32_t>(uvs.size());
  const size_t bytes = sizeof(hdr) + xyz.size() * sizeof(float) +
                       indices.size() * sizeof(uint32_t) +
                       uvs.size() * sizeof(float);
  std::vector<uint8_t> blob(bytes);
  std::memcpy(blob.data(), &hdr, sizeof(hdr));
  uint8_t* p = blob.data() + sizeof(hdr);
  detail::copy_bytes_chunked(p, xyz.data(), xyz.size() * sizeof(float));
  p += xyz.size() * sizeof(float);
  detail::copy_bytes_chunked(p, indices.data(),
                             indices.size() * sizeof(uint32_t));
  p += indices.size() * sizeof(uint32_t);
  if (!uvs.empty()) {
    detail::copy_bytes_chunked(p, uvs.data(), uvs.size() * sizeof(float));
  }
  (void)write_all(disk, blob.data(), blob.size());
}

}  // namespace vista
