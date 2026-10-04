// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/frame/hillshade_bake.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "gis/carto/style/paint_resolve.h"
#include "gis/carto/style/style_types.h"
#include "vista/world/terrain/dem/dem_raster.h"
#include "vista/world/terrain/process/dem_hillshade.h"

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
  slot->opacity = 0.68f;
  slot->texture_key = texture_key;
}

}  // namespace

HillshadeBake bake_hillshade_slot(const std::string& dem_path, double zoom,
                                  const gis::style::StyleLayer& layer,
                                  uint32_t texture_key) {
  HillshadeBake out;
  if (dem_path.empty()) {
    return out;
  }
  const vista::HillshadeParams params = params_for_zoom(zoom, layer);
  double dem_minx = 0;
  double dem_miny = 0;
  double dem_maxx = 0;
  double dem_maxy = 0;
  bool have = cache_lookup(dem_path, params, &out.rgba, &out.width, &out.height,
                           &dem_minx, &dem_miny, &dem_maxx, &dem_maxy);
  if (have) {
    std::fprintf(stderr,
                 "map2d: hillshade cache hit %dx%d max_edge=%d path=%s\n",
                 out.width, out.height, params.max_edge, dem_path.c_str());
  } else {
    vista::DemRaster dem;
    if (!dem.load_gdal_raster(dem_path.c_str()) || dem.empty()) {
      std::fprintf(stderr, "map2d: hillshade skip - DEM load failed (%s)\n",
                   dem_path.c_str());
      return out;
    }
    if (!vista::shade_dem_rgba(dem, params, &out.rgba, &out.width, &out.height) ||
        out.width <= 0 || out.height <= 0 || out.rgba.empty()) {
      out.rgba.clear();
      out.width = 0;
      out.height = 0;
      std::fprintf(stderr, "map2d: hillshade skip - shade_dem_rgba failed\n");
      return out;
    }
    dem.envelope(&dem_minx, &dem_miny, &dem_maxx, &dem_maxy);
    cache_store(dem_path, params, out.rgba, out.width, out.height, dem_minx,
                dem_miny, dem_maxx, dem_maxy);
    have = true;
  }
  if (!have) {
    return out;
  }
  fill_slot(&out.slot, dem_minx, dem_miny, dem_maxx, dem_maxy, texture_key);
  out.ok = true;
  return out;
}

}  // namespace vista
