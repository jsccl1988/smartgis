// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/terrain/dem/dem_bake_cache.h"

#include "vista/world/terrain/dem/dem_raster.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace vista {
namespace {

std::atomic<int64_t> g_load_ms{0};
std::atomic<int64_t> g_tess_ms{0};
std::atomic<int64_t> g_hypso_ms{0};
std::atomic<int> g_load_hit{0};
std::atomic<int> g_hypso_hit{0};

struct FileStamp {
  uint64_t size = 0;
  int64_t mtime = 0;
  bool ok = false;
};

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
  // FNV-1a 64 — stable across processes for cache filenames.
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
  if (const char* e = std::getenv("SMT_DEM_BAKE_CACHE")) {
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

bool ensure_dir(const std::string& dir) {
  if (dir.empty()) {
    return false;
  }
  // Create parent chain (out → data → cache → dem_bake).
  std::string cur;
  for (size_t i = 0; i < dir.size(); ++i) {
    const char c = dir[i];
    cur.push_back(c);
    const bool sep = (c == '\\' || c == '/');
    const bool last = (i + 1 == dir.size());
    if (!sep && !last) {
      continue;
    }
    // Skip drive roots like "C:\"
    if (cur.size() <= 3 && cur.find(':') != std::string::npos) {
      continue;
    }
    CreateDirectoryA(cur.c_str(), nullptr);
  }
  return true;
}

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

bool read_all(const std::string& path, std::vector<uint8_t>* out) {
  if (!out) {
    return false;
  }
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) {
    return false;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    std::fclose(f);
    return false;
  }
  const long sz = ftell(f);
  if (sz <= 0) {
    std::fclose(f);
    return false;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    std::fclose(f);
    return false;
  }
  out->resize(static_cast<size_t>(sz));
  const size_t n =
      std::fread(out->data(), 1, out->size(), f);
  std::fclose(f);
  return n == out->size();
}

bool write_all(const std::string& path, const void* data, size_t bytes) {
  if (!data || bytes == 0) {
    return false;
  }
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
    return false;
  }
  const size_t n = std::fwrite(data, 1, bytes, f);
  std::fclose(f);
  return n == bytes;
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

DemPhaseSample dem_last_phase_sample() {
  DemPhaseSample s;
  s.load_ms = g_load_ms.load(std::memory_order_relaxed);
  s.tess_ms = g_tess_ms.load(std::memory_order_relaxed);
  s.hypso_ms = g_hypso_ms.load(std::memory_order_relaxed);
  s.load_cache_hit = g_load_hit.load(std::memory_order_relaxed);
  s.hypso_cache_hit = g_hypso_hit.load(std::memory_order_relaxed);
  return s;
}

void reset_dem_phase_sample() {
  g_load_ms.store(0, std::memory_order_relaxed);
  g_tess_ms.store(0, std::memory_order_relaxed);
  g_hypso_ms.store(0, std::memory_order_relaxed);
  g_load_hit.store(0, std::memory_order_relaxed);
  g_hypso_hit.store(0, std::memory_order_relaxed);
}

void note_dem_phase_load(int64_t ms, bool cache_hit) {
  g_load_ms.fetch_add(ms, std::memory_order_relaxed);
  if (cache_hit) {
    g_load_hit.fetch_add(1, std::memory_order_relaxed);
  }
}

void note_dem_phase_tess(int64_t ms) {
  g_tess_ms.fetch_add(ms, std::memory_order_relaxed);
}

void note_dem_phase_hypso(int64_t ms, bool cache_hit) {
  g_hypso_ms.fetch_add(ms, std::memory_order_relaxed);
  if (cache_hit) {
    g_hypso_hit.fetch_add(1, std::memory_order_relaxed);
  }
}

bool dem_raster_cache_try_get(const char* path, DemRaster* out) {
  if (!path || !path[0] || !out) {
    return false;
  }
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
  std::memcpy(heights.data(), p, cells * sizeof(float));
  p += cells * sizeof(float);
  std::memcpy(land.data(), p, cells);
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
  std::memcpy(p, heights->data(), cells * sizeof(float));
  p += cells * sizeof(float);
  if (land && land->size() == cells) {
    std::memcpy(p, land->data(), cells);
  } else {
    std::memset(p, 1, cells);
  }
  (void)write_all(disk, blob.data(), blob.size());
}

bool dem_hypso_cache_try_get(const char* path, int max_edge,
                             std::vector<uint8_t>* rgba, int* out_w,
                             int* out_h) {
  if (!path || !path[0] || !rgba || max_edge < 2) {
    return false;
  }
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
  rgba->assign(blob.begin() + static_cast<std::ptrdiff_t>(sizeof(hdr)),
               blob.begin() + static_cast<std::ptrdiff_t>(need));
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

bool dem_mesh_cache_try_get(const char* path, int max_edge, double minx,
                            double miny, double maxx, double maxy, bool windowed,
                            bool apply_land_mask, std::vector<float>* xyz,
                            std::vector<uint32_t>* indices,
                            std::vector<float>* uvs) {
  if (!path || !path[0] || !xyz || !indices || max_edge < 2) {
    return false;
  }
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
  const std::string disk =
      mesh_disk_path(path, max_edge, windowed, apply_land_mask, minx, miny,
                     maxx, maxy);
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
  if (windowed &&
      (hdr.minx != minx || hdr.miny != miny || hdr.maxx != maxx ||
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
  std::memcpy(xyz->data(), p, hdr.xyz_n * sizeof(float));
  p += hdr.xyz_n * sizeof(float);
  indices->resize(hdr.idx_n);
  std::memcpy(indices->data(), p, hdr.idx_n * sizeof(uint32_t));
  p += hdr.idx_n * sizeof(uint32_t);
  if (uvs) {
    uvs->resize(hdr.uv_n);
    if (hdr.uv_n > 0) {
      std::memcpy(uvs->data(), p, hdr.uv_n * sizeof(float));
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
  const std::string disk =
      mesh_disk_path(path, max_edge, windowed, apply_land_mask, minx, miny,
                     maxx, maxy);
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
  std::memcpy(p, xyz.data(), xyz.size() * sizeof(float));
  p += xyz.size() * sizeof(float);
  std::memcpy(p, indices.data(), indices.size() * sizeof(uint32_t));
  p += indices.size() * sizeof(uint32_t);
  if (!uvs.empty()) {
    std::memcpy(p, uvs.data(), uvs.size() * sizeof(float));
  }
  (void)write_all(disk, blob.data(), blob.size());
}

void dem_hypso_cache_put(const char* path, int max_edge,
                         const std::vector<uint8_t>& rgba, int w, int h) {
  if (!path || !path[0] || rgba.empty() || w < 2 || h < 2 || max_edge < 2) {
    return;
  }
  if (rgba.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 4u) {
    return;
  }
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
  const size_t pix =
      static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
  std::vector<uint8_t> blob(sizeof(hdr) + pix);
  std::memcpy(blob.data(), &hdr, sizeof(hdr));
  std::memcpy(blob.data() + sizeof(hdr), rgba.data(), pix);
  (void)write_all(disk, blob.data(), blob.size());
}

namespace {

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
  std::memcpy(seed.xyz.data(), p, hdr.xyz_n * sizeof(float));
  p += hdr.xyz_n * sizeof(float);
  seed.indices.resize(hdr.idx_n);
  std::memcpy(seed.indices.data(), p, hdr.idx_n * sizeof(uint32_t));
  p += hdr.idx_n * sizeof(uint32_t);
  seed.uvs.resize(hdr.uv_n);
  if (hdr.uv_n > 0) {
    std::memcpy(seed.uvs.data(), p, hdr.uv_n * sizeof(float));
    p += hdr.uv_n * sizeof(float);
  }
  seed.rgba.resize(hdr.rgba_n);
  if (hdr.rgba_n > 0) {
    std::memcpy(seed.rgba.data(), p, hdr.rgba_n);
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
  std::memcpy(p, seed.xyz.data(), seed.xyz.size() * sizeof(float));
  p += seed.xyz.size() * sizeof(float);
  std::memcpy(p, seed.indices.data(), seed.indices.size() * sizeof(uint32_t));
  p += seed.indices.size() * sizeof(uint32_t);
  if (!seed.uvs.empty()) {
    std::memcpy(p, seed.uvs.data(), seed.uvs.size() * sizeof(float));
    p += seed.uvs.size() * sizeof(float);
  }
  if (!seed.rgba.empty()) {
    std::memcpy(p, seed.rgba.data(), seed.rgba.size());
  }
  (void)write_all(disk, blob.data(), blob.size());
}

}  // namespace vista
