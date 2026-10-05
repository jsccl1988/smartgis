// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_STORMSURGE_SEED_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_STORMSURGE_SEED_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace app {

class Browser;

namespace detail {

// Wuhan / mid-Yangtze pad used for Scene3D orbit and china_dem analysis crop.
inline constexpr double kStormSurgeMinLon = 114.15;
inline constexpr double kStormSurgeMinLat = 30.45;
inline constexpr double kStormSurgeMaxLon = 114.45;
inline constexpr double kStormSurgeMaxLat = 30.65;
inline constexpr double kStormSurgeSeedLon = 114.30;
inline constexpr double kStormSurgeSeedLat = 30.55;

// Resolve a leaf under exe (..\\data\\plugin\\ or data\\plugin\\).
bool resolve_stormsurge_sample(const wchar_t* leaf, char* out_utf8,
                               size_t out_cap);

// Prefer a china_dem window crop for analysis; fall back to schematic sample.
// Coast GeoJSON is always the plugin fixture. Marks stormsurge-sample-fail
// when neither DEM nor coast can be resolved.
bool resolve_stormsurge_inputs(char* dem_utf8, size_t dem_cap,
                               char* coast_utf8, size_t coast_cap);

// Resolve mask TIFF output path under exe (..\\data\\plugin\\stormsurge_mask.tif).
bool resolve_stormsurge_mask_output(char* out_utf8, size_t out_cap);

// load_coast + stormsurge.run on the analysis DEM (real crop or schematic).
bool seed_stormsurge_processing(Browser& browser, const char* dem_utf8,
                                const char* coast_utf8, const char* out_utf8);

// Windowed china_rs / china_imagery RGBA for draping the 2D map on the DEM.
bool load_stormsurge_map_drape(std::vector<uint8_t>* rgba, int* width,
                               int* height);

// Load a 24/32-bpp BMP as tightly packed RGBA8 (north-up).
bool load_stormsurge_bmp_rgba(const char* path, std::vector<uint8_t>* rgba,
                              int* width, int* height);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_STORMSURGE_SEED_H_
