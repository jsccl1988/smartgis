// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/hillshade_bake.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "base/process/switches.h"
#include "base/trace/event/process_trace.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/style_types.h"
#include "vista/terrain/dem/dem_raster.h"
#include "vista/terrain/process/bake_backend.h"
#include "vista/terrain/process/dem_hillshade.h"

namespace vista {
namespace {

int hillshade_max_edge_for_zoom(double zoom) {
  // Overview / china framing: downsample DEM before shade.
  // china_dem defaults are 1536×960; keep the national frame near that grid.
  if (zoom < 4.5) {
    return 512;
  }
  if (zoom < 7.0) {
    return 768;
  }
  if (zoom < 10.0) {
    return 1024;
  }
  return 1280;
}

vista::HillshadeParams params_for_zoom(double zoom,
                                     const gis::style::StyleLayer& layer) {
  vista::HillshadeParams params;
  params.max_edge = hillshade_max_edge_for_zoom(zoom);
  auto paint_get = [&](const char* key) -> std::string {
    const auto it = layer.paint.find(key);
    return it == layer.paint.end() ? std::string() : it->second;
  };
  if (const std::string s = paint_get("hillshade-illumination-direction");
      !s.empty()) {
    params.illumination_direction_deg = std::strtof(s.c_str(), nullptr);
  }
  if (const std::string s = paint_get("hillshade-exaggeration"); !s.empty()) {
    params.exaggeration = std::strtof(s.c_str(), nullptr);
  }
  uint32_t argb = 0;
  if (gis::style::parse_color(paint_get("hillshade-shadow-color"), &argb)) {
    params.shadow_argb = argb;
  }
  if (gis::style::parse_color(paint_get("hillshade-highlight-color"), &argb)) {
    params.highlight_argb = argb;
  }
  if (gis::style::parse_color(paint_get("hillshade-accent-color"), &argb)) {
    params.accent_argb = argb;
  }
  return params;
}

// Process-wide bake cache: DEM path + illumination + max_edge (viewport LOD).
struct HillshadeBakeCache {
  std::mutex mu;
  std::string dem_path;
  float illumination_direction_deg = 0.f;
  float illumination_altitude_deg = 0.f;
  float exaggeration = 0.f;
  uint32_t shadow_argb = 0;
  uint32_t highlight_argb = 0;
  uint32_t accent_argb = 0;
  int max_edge = 0;
  std::vector<uint8_t> rgba;
  int w = 0;
  int h = 0;
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  bool ready = false;
};

HillshadeBakeCache& hillshade_bake_cache() {
  static HillshadeBakeCache cache;
  return cache;
}

bool cache_lookup(const std::string& dem_path, const vista::HillshadeParams& params,
                  std::vector<uint8_t>* rgba, int* w, int* h, double* min_x,
                  double* min_y, double* max_x, double* max_y) {
  HillshadeBakeCache& c = hillshade_bake_cache();
  std::lock_guard<std::mutex> lock(c.mu);
  if (!c.ready || c.dem_path != dem_path || c.max_edge != params.max_edge ||
      c.illumination_direction_deg != params.illumination_direction_deg ||
      c.illumination_altitude_deg != params.illumination_altitude_deg ||
      c.exaggeration != params.exaggeration ||
      c.shadow_argb != params.shadow_argb ||
      c.highlight_argb != params.highlight_argb ||
      c.accent_argb != params.accent_argb) {
    return false;
  }
  *rgba = c.rgba;
  *w = c.w;
  *h = c.h;
  *min_x = c.min_x;
  *min_y = c.min_y;
  *max_x = c.max_x;
  *max_y = c.max_y;
  return !rgba->empty() && *w > 0 && *h > 0;
}

constexpr char kDiskMagic[8] = {'S', 'G', 'H', 'S', '2', '\0', '\0', '\0'};

struct DiskHeader {
  char magic[8];
  int32_t w = 0;
  int32_t h = 0;
  int32_t max_edge = 0;
  float illumination_direction_deg = 0.f;
  float illumination_altitude_deg = 0.f;
  float exaggeration = 0.f;
  uint32_t shadow_argb = 0;
  uint32_t highlight_argb = 0;
  uint32_t accent_argb = 0;
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
};

std::string disk_cache_path(const std::string& dem_path,
                            const vista::HillshadeParams& params) {
  char suffix[80];
  std::snprintf(suffix, sizeof(suffix), ".hs_%d_%.0f.bin", params.max_edge,
                params.illumination_direction_deg);
  return dem_path + suffix;
}

bool params_match_header(const DiskHeader& hdr,
                         const vista::HillshadeParams& params) {
  return hdr.max_edge == params.max_edge &&
         hdr.illumination_direction_deg == params.illumination_direction_deg &&
         hdr.illumination_altitude_deg == params.illumination_altitude_deg &&
         hdr.exaggeration == params.exaggeration &&
         hdr.shadow_argb == params.shadow_argb &&
         hdr.highlight_argb == params.highlight_argb &&
         hdr.accent_argb == params.accent_argb;
}

bool disk_lookup(const std::string& dem_path, const vista::HillshadeParams& params,
                 std::vector<uint8_t>* rgba, int* w, int* h, double* min_x,
                 double* min_y, double* max_x, double* max_y) {
  const std::string path = disk_cache_path(dem_path, params);
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  DiskHeader hdr{};
  in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
  if (!in || std::memcmp(hdr.magic, kDiskMagic, 8) != 0 || hdr.w <= 0 ||
      hdr.h <= 0 || !params_match_header(hdr, params)) {
    return false;
  }
  const size_t nbytes =
      static_cast<size_t>(hdr.w) * static_cast<size_t>(hdr.h) * 4u;
  std::vector<uint8_t> buf(nbytes);
  in.read(reinterpret_cast<char*>(buf.data()),
          static_cast<std::streamsize>(nbytes));
  if (!in || static_cast<size_t>(in.gcount()) != nbytes) {
    return false;
  }
  *rgba = std::move(buf);
  *w = hdr.w;
  *h = hdr.h;
  *min_x = hdr.min_x;
  *min_y = hdr.min_y;
  *max_x = hdr.max_x;
  *max_y = hdr.max_y;
  return true;
}

void disk_store(const std::string& dem_path, const vista::HillshadeParams& params,
                const std::vector<uint8_t>& rgba, int w, int h, double min_x,
                double min_y, double max_x, double max_y) {
  if (rgba.empty() || w <= 0 || h <= 0) {
    return;
  }
  const size_t nbytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
  if (rgba.size() < nbytes) {
    return;
  }
  const std::string path = disk_cache_path(dem_path, params);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return;
  }
  DiskHeader hdr{};
  std::memcpy(hdr.magic, kDiskMagic, 8);
  hdr.w = w;
  hdr.h = h;
  hdr.max_edge = params.max_edge;
  hdr.illumination_direction_deg = params.illumination_direction_deg;
  hdr.illumination_altitude_deg = params.illumination_altitude_deg;
  hdr.exaggeration = params.exaggeration;
  hdr.shadow_argb = params.shadow_argb;
  hdr.highlight_argb = params.highlight_argb;
  hdr.accent_argb = params.accent_argb;
  hdr.min_x = min_x;
  hdr.min_y = min_y;
  hdr.max_x = max_x;
  hdr.max_y = max_y;
  out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
  out.write(reinterpret_cast<const char*>(rgba.data()),
            static_cast<std::streamsize>(nbytes));
}

void cache_store(const std::string& dem_path, const vista::HillshadeParams& params,
                 std::vector<uint8_t> rgba, int w, int h, double min_x,
                 double min_y, double max_x, double max_y) {
  HillshadeBakeCache& c = hillshade_bake_cache();
  std::lock_guard<std::mutex> lock(c.mu);
  c.dem_path = dem_path;
  c.illumination_direction_deg = params.illumination_direction_deg;
  c.illumination_altitude_deg = params.illumination_altitude_deg;
  c.exaggeration = params.exaggeration;
  c.shadow_argb = params.shadow_argb;
  c.highlight_argb = params.highlight_argb;
  c.accent_argb = params.accent_argb;
  c.max_edge = params.max_edge;
  c.rgba = std::move(rgba);
  c.w = w;
  c.h = h;
  c.min_x = min_x;
  c.min_y = min_y;
  c.max_x = max_x;
  c.max_y = max_y;
  c.ready = !c.rgba.empty() && w > 0 && h > 0;
}

void fill_slot(TileSlot* slot, double min_x, double min_y, double max_x,
               double max_y, uint32_t texture_key) {
  slot->min_x = min_x;
  slot->max_x = max_x;
  slot->min_y = min_y;
  slot->max_y = max_y;
  // Multiply strength for crisp DEM relief after the hi-res china_dem bake.
  slot->opacity = 0.82f;
  slot->texture_key = texture_key;
}

int64_t elapsed_ms(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - t0)
      .count();
}

bool disk_enabled() {
  if (const char* s = base::switch_cstr("bake-disk"); s && s[0] == '0' &&
      s[1] == '\0') {
    return false;
  }
  return bake_disk_enabled_from_env();
}

}  // namespace

namespace {

std::atomic<int64_t> g_mem_ms{0};
std::atomic<int64_t> g_disk_ms{0};
std::atomic<int64_t> g_load_ms{0};
std::atomic<int64_t> g_shade_ms{0};
std::atomic<int64_t> g_store_ms{0};
std::atomic<int> g_mem_hit{0};
std::atomic<int> g_disk_hit{0};
std::atomic<int> g_used_cuda{0};
std::atomic<int> g_w{0};
std::atomic<int> g_h{0};
std::atomic<int> g_max_edge{0};

void publish_sample(const HillshadeBakeSample& s) {
  g_mem_ms.store(s.mem_ms, std::memory_order_relaxed);
  g_disk_ms.store(s.disk_ms, std::memory_order_relaxed);
  g_load_ms.store(s.load_ms, std::memory_order_relaxed);
  g_shade_ms.store(s.shade_ms, std::memory_order_relaxed);
  g_store_ms.store(s.store_ms, std::memory_order_relaxed);
  g_mem_hit.store(s.mem_hit, std::memory_order_relaxed);
  g_disk_hit.store(s.disk_hit, std::memory_order_relaxed);
  g_used_cuda.store(s.used_cuda, std::memory_order_relaxed);
  g_w.store(s.width, std::memory_order_relaxed);
  g_h.store(s.height, std::memory_order_relaxed);
  g_max_edge.store(s.max_edge, std::memory_order_relaxed);
}

}  // namespace

HillshadeBake bake_hillshade_slot(const std::string& dem_path, double zoom,
                                  const gis::style::StyleLayer& layer,
                                  uint32_t texture_key) {
  BASE_TRACE_EVENT("HillshadeBake", "bake");
  HillshadeBake out;
  HillshadeBakeSample sample;
  if (dem_path.empty()) {
    publish_sample(sample);
    return out;
  }
  const vista::HillshadeParams params = params_for_zoom(zoom, layer);
  sample.max_edge = params.max_edge;
  double dem_minx = 0;
  double dem_miny = 0;
  double dem_maxx = 0;
  double dem_maxy = 0;
  auto t = std::chrono::steady_clock::now();
  {
    BASE_TRACE_EVENT("cache_mem", "bake");
    bool have = cache_lookup(dem_path, params, &out.rgba, &out.width,
                             &out.height, &dem_minx, &dem_miny, &dem_maxx,
                             &dem_maxy);
    sample.mem_ms = elapsed_ms(t);
    if (have) {
      sample.mem_hit = 1;
      std::fprintf(stderr,
                   "map2d: hillshade cache hit %dx%d max_edge=%d path=%s\n",
                   out.width, out.height, params.max_edge, dem_path.c_str());
      fill_slot(&out.slot, dem_minx, dem_miny, dem_maxx, dem_maxy, texture_key);
      out.ok = true;
      sample.width = out.width;
      sample.height = out.height;
      publish_sample(sample);
      return out;
    }
  }
  if (disk_enabled()) {
    t = std::chrono::steady_clock::now();
    BASE_TRACE_EVENT("cache_disk", "bake");
    if (disk_lookup(dem_path, params, &out.rgba, &out.width, &out.height,
                    &dem_minx, &dem_miny, &dem_maxx, &dem_maxy)) {
      sample.disk_ms = elapsed_ms(t);
      sample.disk_hit = 1;
      t = std::chrono::steady_clock::now();
      cache_store(dem_path, params, out.rgba, out.width, out.height, dem_minx,
                  dem_miny, dem_maxx, dem_maxy);
      sample.store_ms = elapsed_ms(t);
      std::fprintf(stderr,
                   "map2d: hillshade disk hit %dx%d max_edge=%d path=%s\n",
                   out.width, out.height, params.max_edge, dem_path.c_str());
      fill_slot(&out.slot, dem_minx, dem_miny, dem_maxx, dem_maxy, texture_key);
      out.ok = true;
      sample.width = out.width;
      sample.height = out.height;
      publish_sample(sample);
      return out;
    }
    sample.disk_ms = elapsed_ms(t);
  }
  vista::DemRaster dem;
  t = std::chrono::steady_clock::now();
  {
    BASE_TRACE_EVENT("load_dem", "bake");
    if (!dem.load_gdal_raster(dem_path.c_str()) || dem.empty()) {
      sample.load_ms = elapsed_ms(t);
      std::fprintf(stderr, "map2d: hillshade skip - DEM load failed (%s)\n",
                   dem_path.c_str());
      publish_sample(sample);
      return out;
    }
  }
  sample.load_ms = elapsed_ms(t);
  reset_last_shade_used_cuda();
  t = std::chrono::steady_clock::now();
  if (!vista::shade_dem_rgba(dem, params, &out.rgba, &out.width, &out.height) ||
      out.width <= 0 || out.height <= 0 || out.rgba.empty()) {
    sample.shade_ms = elapsed_ms(t);
    sample.used_cuda = last_shade_used_cuda();
    out.rgba.clear();
    out.width = 0;
    out.height = 0;
    std::fprintf(stderr, "map2d: hillshade skip - shade_dem_rgba failed\n");
    publish_sample(sample);
    return out;
  }
  sample.shade_ms = elapsed_ms(t);
  sample.used_cuda = last_shade_used_cuda();
  dem.envelope(&dem_minx, &dem_miny, &dem_maxx, &dem_maxy);
  t = std::chrono::steady_clock::now();
  {
    BASE_TRACE_EVENT("disk_store", "bake");
    cache_store(dem_path, params, out.rgba, out.width, out.height, dem_minx,
                dem_miny, dem_maxx, dem_maxy);
    if (disk_enabled()) {
      disk_store(dem_path, params, out.rgba, out.width, out.height, dem_minx,
                 dem_miny, dem_maxx, dem_maxy);
    }
  }
  sample.store_ms = elapsed_ms(t);
  fill_slot(&out.slot, dem_minx, dem_miny, dem_maxx, dem_maxy, texture_key);
  out.ok = true;
  sample.width = out.width;
  sample.height = out.height;
  publish_sample(sample);
  return out;
}

HillshadeBakeSample hillshade_last_bake_sample() {
  HillshadeBakeSample s;
  s.mem_ms = g_mem_ms.load(std::memory_order_relaxed);
  s.disk_ms = g_disk_ms.load(std::memory_order_relaxed);
  s.load_ms = g_load_ms.load(std::memory_order_relaxed);
  s.shade_ms = g_shade_ms.load(std::memory_order_relaxed);
  s.store_ms = g_store_ms.load(std::memory_order_relaxed);
  s.mem_hit = g_mem_hit.load(std::memory_order_relaxed);
  s.disk_hit = g_disk_hit.load(std::memory_order_relaxed);
  s.used_cuda = g_used_cuda.load(std::memory_order_relaxed);
  s.width = g_w.load(std::memory_order_relaxed);
  s.height = g_h.load(std::memory_order_relaxed);
  s.max_edge = g_max_edge.load(std::memory_order_relaxed);
  return s;
}

void reset_hillshade_bake_sample() {
  HillshadeBakeSample z;
  publish_sample(z);
}

void reset_hillshade_bake_cache() {
  HillshadeBakeCache& c = hillshade_bake_cache();
  std::lock_guard<std::mutex> lock(c.mu);
  c.ready = false;
  c.rgba.clear();
  c.w = 0;
  c.h = 0;
  c.dem_path.clear();
}

}  // namespace vista
