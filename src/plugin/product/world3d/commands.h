// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_COMMANDS_H_
#define PLUGIN_WORLD3D_COMMANDS_H_

#include <cstdint>
#include <functional>
#include <string>

namespace content {
class PluginHost;
}

namespace plugin {

// Views map commits the loaded TIN/grid. Unset writer keeps no_map_seam.
using World3dSurfaceWriter = std::function<bool(
    const double* xyz, int point_count, const int* triangles,
    int triangle_count, const char* op)>;
void set_world3d_surface_writer(World3dSurfaceWriter writer);

// Scene-device write seam for former model3d ops + True Earth browse.
// App shell installs when a real scene / map stand-in exists; unset callbacks
// keep no_scene_device.
struct World3dSceneWriter {
  std::function<bool(const std::string& path)> add_pointcloud;
  // Optional in-memory attach (PDAL / preloaded buffers). xyz interleaved;
  // rgba may be null or 4 * point_count (RGBA8).
  std::function<bool(const float* xyz, int point_count, const uint8_t* rgba)>
      add_pointcloud_xyz;
  std::function<bool()> add_sphere;
  std::function<bool()> add_water;
  std::function<bool()> add_terrain_grid;
  std::function<bool()> add_terrain_tin;
  std::function<bool()> layer_points_to_3d;
  std::function<bool()> layer_lines_to_3d;
  std::function<bool()> layer_polygons_to_3d;
  std::function<bool()> create_tin_from_active_layer;
  // Google-Earth-class MVP: Scene3D tab + China DEM + atmosphere product
  // defaults. Optional empty path for attach uses shipped m3 city fixture.
  std::function<bool()> open_earth;
  // Local lon/lat extent reframe + orbit distance (span_deg half-box).
  std::function<bool(double lon, double lat, float distance, double span_deg)>
      fly_to;
  // Attach 3D Tiles tileset JSON (city fixture or user path).
  std::function<bool(const std::string& tileset_json_path)> attach_tileset;
  // Global / custom DEM GeoTIFF. Empty path → resolve default locations
  // (out/data/global_dem.tif, plugins/world3d/data/, else china stand-in).
  // Returns false only on hard failure; missing global file may still succeed
  // with china_dem as documented stand-in (result JSON carries source).
  std::function<bool(const std::string& dem_path, std::string* result_json)>
      load_global_dem;
  // Satellite cloud cover field. Empty path → procedural atmosphere clouds.
  // Non-empty → AtmosphereSession::load_fields("|path|:cloud_cover").
  std::function<bool(const std::string& imagery_path, bool enabled,
                     std::string* result_json)>
      set_satellite_cloud;
  // Explicit atmosphere pass toggles (sky / ocean / cloud / fog).
  std::function<bool(bool sky, bool ocean, bool cloud, bool fog)>
      set_atmosphere;
};

void set_world3d_scene_writer(World3dSceneWriter writer);

// Registers former model3d command/processing ids under smartgis.world3d.
bool register_world3d_scene_ops(content::PluginHost* host);

bool register_world3d(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_COMMANDS_H_
