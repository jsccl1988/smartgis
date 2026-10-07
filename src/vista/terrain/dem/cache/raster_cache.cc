// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/dem_bake_cache.h"

#include "vista/terrain/dem/cache/io.h"
#include "vista/terrain/dem/raster/dem_raster.h"

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

struct RasterMemCache {
  std::mutex mu;
  std::string path;
  FileStamp stamp;
  int cols = 0;
  int rows = 0;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  float min_m = 0;
  float max_m = 1;
  float vert_exag = 0.0012f;
  std::vector<float> heights;
  std::vector<uint8_t> land;
  bool ready = false;
};

RasterMemCache& raster_mem() {
  static RasterMemCache c;
  return c;
}

#pragma pack(push, 1)
struct RasterDiskHdr {
  char magic[8];
  uint64_t path_hash;
  uint64_t file_size;
  int64_t file_mtime;
  int32_t cols;
  int32_t rows;
  double minx;
  double miny;
  double maxx;
  double maxy;
  float min_m;
  float max_m;
  float vert_exag;
};
#pragma pack(pop)

std::string raster_disk_path(const char* path) {
  const std::string root = cache_root_dir();
  if (root.empty()) {
    return {};
  }
  char name[64];
  std::snprintf(name, sizeof(name), "%016llx_raster.bin",
                static_cast<unsigned long long>(path_hash64(path)));
  return root + "\\" + name;
}

}  // namespace

bool dem_raster_cache_try_get(const char* path, DemRaster* out) {
  if (!path || !path[0] || !out) {
    return false;
  }
  warmup_dem_bake_cache();
  const FileStamp stamp = file_stamp(path);
  if (!stamp.ok) {
    return false;
  }

  {
    RasterMemCache& c = raster_mem();
    std::lock_guard<std::mutex> lock(c.mu);
    if (c.ready && c.path == path && c.stamp.size == stamp.size &&
        c.stamp.mtime == stamp.mtime && !c.heights.empty()) {
      if (!out->adopt_bake_cache(c.cols, c.rows, c.minx, c.miny, c.maxx, c.maxy,
                                 c.min_m, c.max_m, c.vert_exag, c.heights,
                                 c.land, path)) {
        return false;
      }
      return true;
    }
  }

  const std::string disk = raster_disk_path(path);
  if (disk.empty()) {
    return false;
  }
  std::vector<uint8_t> blob;
  if (!read_all(disk, &blob) || blob.size() < sizeof(RasterDiskHdr)) {
    return false;
  }
  RasterDiskHdr hdr = {};
  std::memcpy(&hdr, blob.data(), sizeof(hdr));
  if (std::memcmp(hdr.magic, "SGDEM1\0\0", 8) != 0 ||
      hdr.path_hash != path_hash64(path) || hdr.file_size != stamp.size ||
      hdr.file_mtime != stamp.mtime || hdr.cols < 2 || hdr.rows < 2) {
    return false;
  }
  const size_t cells =
      static_cast<size_t>(hdr.cols) * static_cast<size_t>(hdr.rows);
  const size_t need =
      sizeof(RasterDiskHdr) + cells * sizeof(float) + cells * sizeof(uint8_t);
  if (blob.size() < need) {
    return false;
  }
  std::vector<float> heights(cells);
  std::vector<uint8_t> land(cells);
  const uint8_t* p = blob.data() + sizeof(RasterDiskHdr);
  detail::copy_bytes_chunked(heights.data(), p, cells * sizeof(float));
  p += cells * sizeof(float);
  detail::copy_bytes_chunked(land.data(), p, cells);
  if (!out->adopt_bake_cache(hdr.cols, hdr.rows, hdr.minx, hdr.miny, hdr.maxx,
                             hdr.maxy, hdr.min_m, hdr.max_m, hdr.vert_exag,
                             std::move(heights), std::move(land), path)) {
    return false;
  }

  RasterMemCache& c = raster_mem();
  std::lock_guard<std::mutex> lock(c.mu);
  c.path = path;
  c.stamp = stamp;
  c.cols = hdr.cols;
  c.rows = hdr.rows;
  c.minx = hdr.minx;
  c.miny = hdr.miny;
  c.maxx = hdr.maxx;
  c.maxy = hdr.maxy;
  c.min_m = hdr.min_m;
  c.max_m = hdr.max_m;
  c.vert_exag = hdr.vert_exag;
  int cols = 0;
  int rows = 0;
  const std::vector<float>* hp = nullptr;
  const std::vector<uint8_t>* lp = nullptr;
  out->export_bake_cache(&cols, &rows, &c.minx, &c.miny, &c.maxx, &c.maxy,
                         &c.min_m, &c.max_m, &c.vert_exag, &hp, &lp);
  if (hp) {
    c.heights = *hp;
  }
  if (lp) {
    c.land = *lp;
  }
  c.ready = !c.heights.empty();
  return true;
}

void dem_raster_cache_put(const char* path, const DemRaster& dem) {
  if (!path || !path[0] || dem.empty()) {
    return;
  }
  warmup_dem_bake_cache();
  const FileStamp stamp = file_stamp(path);
  if (!stamp.ok) {
    return;
  }
  int cols = 0;
  int rows = 0;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  float min_m = 0;
  float max_m = 1;
  float vert_exag = 0.0012f;
  const std::vector<float>* heights = nullptr;
  const std::vector<uint8_t>* land = nullptr;
  dem.export_bake_cache(&cols, &rows, &minx, &miny, &maxx, &maxy, &min_m,
                        &max_m, &vert_exag, &heights, &land);
  if (!heights || heights->empty() || cols < 2 || rows < 2) {
    return;
  }

  {
    RasterMemCache& c = raster_mem();
    std::lock_guard<std::mutex> lock(c.mu);
    c.path = path;
    c.stamp = stamp;
    c.cols = cols;
    c.rows = rows;
    c.minx = minx;
    c.miny = miny;
    c.maxx = maxx;
    c.maxy = maxy;
    c.min_m = min_m;
    c.max_m = max_m;
    c.vert_exag = vert_exag;
    c.heights = *heights;
    if (land) {
      c.land = *land;
    } else {
      c.land.clear();
    }
    c.ready = true;
  }

  const std::string root = cache_root_dir();
  const std::string disk = raster_disk_path(path);
  if (root.empty() || disk.empty() || !ensure_dir(root)) {
    return;
  }
  const size_t cells = heights->size();
  std::vector<uint8_t> blob(sizeof(RasterDiskHdr) + cells * sizeof(float) +
                            cells);
  RasterDiskHdr hdr = {};
  std::memcpy(hdr.magic, "SGDEM1\0\0", 8);
  hdr.path_hash = path_hash64(path);
  hdr.file_size = stamp.size;
  hdr.file_mtime = stamp.mtime;
  hdr.cols = cols;
  hdr.rows = rows;
  hdr.minx = minx;
  hdr.miny = miny;
  hdr.maxx = maxx;
  hdr.maxy = maxy;
  hdr.min_m = min_m;
  hdr.max_m = max_m;
  hdr.vert_exag = vert_exag;
  std::memcpy(blob.data(), &hdr, sizeof(hdr));
  uint8_t* p = blob.data() + sizeof(hdr);
  detail::copy_bytes_chunked(p, heights->data(), cells * sizeof(float));
  p += cells * sizeof(float);
  if (land && land->size() == cells) {
    detail::copy_bytes_chunked(p, land->data(), cells);
  } else {
    std::memset(p, 1, cells);
  }
  (void)write_all(disk, blob.data(), blob.size());
}

}  // namespace vista
